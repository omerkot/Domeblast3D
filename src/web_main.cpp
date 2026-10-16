// Browser platform adapter. Gameplay, geometry, materials, and mixer are shared with native.
#include "audio.hpp"
#include "render.hpp"
#include <emscripten.h>
#include <emscripten/html5.h>
using namespace db;
static Game game;
static Renderer renderer;
static Audio audio;
static SDL_Window *window;
static int screen = 0, selected = 0;
static float elapsed = 0, flatCameraAngle = .45f;
static bool xrMode = false;
// Scene and HUD each have a clip matrix and world-from-eye matrix.
static float xrMatrices[64];
EM_JS(void, publish, (int state, int score, int high, int lives, int level, int peak,
                     int practice, int automatic, int classic, int dying, int locked, float banner), {
  if (Module.onState) Module.onState({screen:state,score,high,lives,level,peak,practice,
    autoFire:!!automatic,classic:!!classic,dying:!!dying,locked:!!locked,banner});
});
EM_JS(void, radarPoint, (int kind, float x, float y), { if(Module.radarPoint) Module.radarPoint(kind,x,y); });
extern "C" {
EMSCRIPTEN_KEEPALIVE int web_audio_running() {
  return EM_ASM_INT({var ctx=Module.SDL2 && Module.SDL2.audioContext;return ctx && ctx.state==='running' ? 1 : 0;});
}
EMSCRIPTEN_KEEPALIVE void web_restore(int high, int automatic, int classic, int master, int music, int effects, int muted) {
  game.high = std::max(0, high); game.autoFire = automatic != 0; game.classic = classic != 0;
  audio.volume = int(clamp(master,0,100)); audio.musicVolume = int(clamp(music,0,100));
  audio.effectsVolume = int(clamp(effects,0,100)); audio.music = !muted;
}
EMSCRIPTEN_KEEPALIVE void web_action(int action, int value) {
  switch(action) {
    case 0: if(screen == 0 || screen == 3) {game.reset(selected); screen=1; audio.push(6);} else if(screen==2) screen=1; break;
    case 1: if(screen==1) screen=2; else if(screen==2) screen=1; break;
    case 2: if(screen==2 || screen==3) {screen=0; game.menuSceneActive=false;} break;
    case 3: game.autoFire=!game.autoFire; break;
    case 4: audio.music=!audio.music.load(); break;
    case 5: if(screen==0) game.classic=!game.classic; break;
    case 6: if(screen==0) selected=int(clamp(value,0,6)); break;
    case 7: audio.volume=int(clamp(value,0,100)); break;
    case 8: audio.musicVolume=int(clamp(value,0,100)); if(value>0) audio.music=1; break;
    case 9: audio.effectsVolume=int(clamp(value,0,100)); break;
    case 10: if(screen==1) screen=2; break;
  }
}
EMSCRIPTEN_KEEPALIVE int web_stat(int key) {
  switch(key) {case 0:return screen; case 1:return game.score;case 2:return game.high;
    case 3:return game.lives; case 4:return game.level;case 5:return selected;
    case 6:return game.autoFire;case 7:return game.classic;case 8:return !audio.music.load();}
  return 0;
}
EMSCRIPTEN_KEEPALIVE void web_update(float dt, float mx, float my, float ax, float ay, float orbit, int fire, int focus) {
  SDL_Event e; while(SDL_PollEvent(&e)) {}
  // Never advance a suspended tab by a large catch-up step.
  dt=clamp(dt,0,.05f); elapsed+=dt;
  game.sounds.clear();
  game.syncTerrain(audio.terrainTimeline.load(),dt,audio.device!=0 && web_audio_running());
  if(screen==1) {
    // Bounded substeps preserve collision behavior on slow displays.
    Input input;input.move=mx;input.movey=my;input.aimx=ax;input.aimy=ay;
    input.orbit=orbit;input.fire=fire;input.focus=focus;
    int steps=std::max(1,int(std::ceil(dt/(1.f/60))));
    for(int i=0;i<steps;i++)game.update(dt/steps,input);
    if(game.over)screen=3;
  } else if(screen==0) game.updateTitle(elapsed,dt,selected);
  audio.level=game.level;audio.shipDucking=game.spaceshipDucking();
  audio.active=screen==1&&!game.dying();audio.sceneVisible=screen==1;
  audio.viewTerrain=game.terrain.packed();audio.viewAngle=game.cameraAngle;audio.viewShake=cameraShake(game);
  if(xrMode)audio.trackedView.terrain=game.terrain;
  auto threat=xrMode?visibleThreatAudio(game,audio.trackedView):visibleThreatAudio(game);audio.whistle=threat.whistle;audio.missileThreat=threat.missile;audio.ufoPresent=threat.ufo;
  for(const auto &s:game.sounds)audio.push(s);
  publish(screen,game.score,game.high,game.lives,game.level,game.peak,game.practice,game.autoFire,game.classic,game.dying(),game.locked,game.banner);
  radarPoint(-2,0,0);
  float ca=std::cos(game.cameraAngle),sa=std::sin(game.cameraAngle);
  for(const auto &e:game.enemies)radarPoint(int(e.kind),(e.p.x*ca-e.p.z*sa)/9.2f,(e.p.x*sa+e.p.z*ca)/9.2f);
  radarPoint(-1,(game.x*ca-game.z*sa)/9.2f,(game.x*sa+game.z*ca)/9.2f);
}
EMSCRIPTEN_KEEPALIVE void web_frame(float dt, float mx, float my, float ax, float ay, float orbit, int fire, int focus, int w, int h) {
  if (xrMode) return;
  renderer.resize(w,h);
  web_update(dt,mx,my,ax,ay,orbit,fire,focus);
  Color sky=skies[game.level-1];
  if(game.dying()){float v=std::min(1.f,game.deathTimer/.4f);sky={sky.r*(1-v)+.48f*v,sky.g*(1-v)+.012f*v,sky.b*(1-v)+.008f*v};}
  renderer.begin(sky);renderer.world(game,game.menuSceneActive);renderer.finish();SDL_GL_SwapWindow(window);
}
EMSCRIPTEN_KEEPALIVE float *web_xr_buffer() { return xrMatrices; }
EMSCRIPTEN_KEEPALIVE void web_xr_mode(int enabled) {
  if (enabled && !xrMode) flatCameraAngle=game.cameraAngle;
  if (!enabled) {if(xrMode)game.cameraAngle=flatCameraAngle;renderer.setColorPipeline(0);}
  xrMode=enabled!=0;
  renderer.useTrackedView=audio.useTrackedView=xrMode;
  if (screen==1) screen=2;
}
EMSCRIPTEN_KEEPALIVE int web_xr_color(int mode) { return renderer.setColorPipeline(mode); }
EMSCRIPTEN_KEEPALIVE void web_xr_pose() {
  audio.trackedView=CameraView::fromTracked(xrMatrices,xrMatrices+16,game.terrain);
  V eye=audio.trackedView.eye;
  // Control directions follow the viewer's position around the stable table.
  game.cameraAngle=std::atan2(eye.x,eye.z);
}
EMSCRIPTEN_KEEPALIVE void web_xr_draw(int framebuffer, int vx, int vy, int vw, int vh,
                                    int renderWidth, int renderHeight, int hudTexture) {
  if (!xrMode) return;
  renderer.resize(renderWidth,renderHeight);
  renderer.trackedView=CameraView::fromTracked(xrMatrices,xrMatrices+16,game.terrain);
  renderer.useTrackedView=true;
  // No screen shake or automatic camera orbit reaches the tracked headset view.
  renderer.begin(skies[game.level-1]);
  renderer.world(game,game.menuSceneActive);
  renderer.finish(true,framebuffer,vx,vy,vw,vh);
  // A real world-space panel remains legible without relying on DOM overlays in VR.
  if (hudTexture) {
    // The tabletop may rotate while the readable HUD stays on its fixed anchor.
    renderer.trackedView=CameraView::fromTracked(xrMatrices+32,xrMatrices+48,game.terrain);
    renderer.camera();
    bool compact=screen==1;
    float left=-7, top=compact?-1.7f:8.f, z=compact?10.5f:7.f;
    float width=14, height=compact?3.5f:10.5f;
    Color white{1,1,1,1};
    glDisable(GL_DEPTH_TEST);glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    renderer.vertex({left,top,z},white,0,0);
    renderer.vertex({left,top-height,z},white,0,1);
    renderer.vertex({left+width,top-height,z},white,1,1);
    renderer.vertex({left,top,z},white,0,0);
    renderer.vertex({left+width,top-height,z},white,1,1);
    renderer.vertex({left+width,top,z},white,1,0);
    renderer.flush(hudTexture,false,true);
    glDisable(GL_BLEND);
  }
}
}
int main() {
  adaptWebViewport=true; // Fit portrait width and retain overhead threats in landscape.
  if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_EVENTS)!=0) {
    EM_ASM({Module.onFailure('The browser could not initialize graphics or audio.');});
    return 1;
  }
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
  // SDL maps GLES 3 to WebGL 2 (required for the original NPOT mipmapped atlas).
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
  EM_ASM({EGL.contextAttributes.xrCompatible=true;});
  window=SDL_CreateWindow("Domeblast 3D",0,0,640,480,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
  if(!window || !SDL_GL_CreateContext(window)) {
    EM_ASM({Module.onFailure('WebGL 2 is unavailable. Enable hardware acceleration or use a WebGL 2 browser.');});
    return 2;
  }
  try {renderer.init();}catch(const std::exception &e){EM_ASM({Module.onFailure(UTF8ToString($0));},e.what());return 3;}
  // Register browser-owned XR objects without ever deleting the compositor framebuffer.
  EM_ASM({
    Module.registerXRFramebuffer=function(fb){var id=GL.getNewId(GL.framebuffers);GL.framebuffers[id]=fb;fb.name=id;return id;};
    Module.releaseXRFramebuffer=function(id){GL.framebuffers[id]=null;};
    Module.registerXRTexture=function(tex){var id=GL.getNewId(GL.textures);GL.textures[id]=tex;tex.name=id;return id;};
    Module.releaseXRTexture=function(id){GL.textures[id]=null;};
  });
  audio.init();game.updateTitle(0,0,0);
  EM_ASM({Module.onReady();});
  return 0;
}
