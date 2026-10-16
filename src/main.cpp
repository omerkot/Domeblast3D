#include "audio.hpp"
#include "game.hpp"
#include "render.hpp"
#include <SDL.h>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
using namespace db;
// Handheld launchers may keep writable state outside the installed assets.
static std::string savePath() {
  const char *path = std::getenv("DOMEBLAST_SAVE_PATH");
  return path && *path ? path : "save.dat";
}
static std::string number(int n) {
  std::string s = std::to_string(n);
  for (int i = int(s.size()) - 3; i > 0; i -= 3)
    s.insert(i, ",");
  return s;
}
static void save(const Game &g, Audio &a) {
  const std::string path = savePath(), temporary = path + ".tmp";
  std::ofstream f(temporary);
  f << g.high << ' ' << a.volume.load() << ' ' << a.music.load() << ' ' << g.classic << ' '
    << g.autoFire << ' ' << a.musicVolume.load() << ' ' << a.effectsVolume.load() << '\n';
  f.close();
  if (f)
    std::rename(temporary.c_str(), path.c_str());
}
static void panel(Renderer &r, Game &g, int screen, int selected, Audio &a, float fps) {
  Color ac = accents[g.level - 1], muted = {.46f, .59f, .69f};
  r.begin({.014f, .023f, .038f}, false);
  r.overlay();
  r.text(24, 18, "DOMEBLAST / MISSION CONTROL", 22);
  r.rect(24, 57, 592, 2, ac);
  r.text(24, 76, screen == 0 ? "PERSONAL BEST" : "SCORE", 14, muted);
  r.text(24, 97, number(screen == 0 ? g.high : g.score), 36);
  r.text(350, 76, "CANNONS", 14, muted);
  r.text(350, 97, screen == 0 ? "05" : std::to_string(g.lives), 36, ac);
  r.text(24, 150, "0" + std::to_string(g.level) + " / " + names[g.level - 1], 19, ac);
  if (screen == 0) {
    r.text(24, 197,
           selected ? "PRACTICE SECTOR " + std::to_string(selected) : "ARCADE / START AT ZERO", 19);
    r.text(24, 229, "D-PAD LEFT / RIGHT: SELECT SECTOR", 14, muted);
    r.text(24, 257,
           "SELECT: MUSIC " + std::string(a.music ? "ON" : "OFF") + "   Y: AUTO " +
               (g.autoFire ? "ON" : "OFF"),
           15);
    r.text(24, 287, "UP / DOWN: VOLUME " + std::to_string(a.volume.load()) + "%", 14, muted);
    r.text(24, 315,
           "R3: " + std::string(g.classic ? "SLOW 1.5 SHOTS/SEC" : "STANDARD 3.1 SHOTS/SEC"), 14,
           ac);
    r.text(24, 340, "L2 / R2: ROTATE CAMERA", 14, muted);
  } else {
    r.text(24, 193, "360 DEGREE RADAR", 14, muted);
    for (int j = 1; j <= 3; j++)
      r.ring({131, 287, 0}, j * 24, 1, {.10f, .23f, .29f}, 0, 48);
    r.rect(58, 286, 146, 1, {.08f, .17f, .23f});
    r.rect(130, 214, 1, 146, {.08f, .17f, .23f});
    float ca = std::cos(g.cameraAngle), sa = std::sin(g.cameraAngle);
    auto radar = [&](V p) {
      return V{131 + (p.x * ca - p.z * sa) * 7.5f, 287 + (p.x * sa + p.z * ca) * 7.5f, 0};
    };
    for (const auto &e : g.enemies) {
      V p = radar(e.p);
      Color c =
          e.kind == Spinner || e.kind == SmallSpinner ? Color{1, .3f, .16f} : Color{.65f, .85f, 1};
      r.rect(p.x - 2, p.y - 2, 4, 4, c);
    }
    V player = radar({g.x, 0, g.z});
    r.rect(player.x - 4, player.y - 4, 8, 8, {1, 1, 1});
    r.text(257, 212,
           g.dying()  ? "CANNON LOST"
           : g.locked ? "TARGET LOCKED"
                      : "AIM TO LOCK",
           19, g.locked ? Color{1, .83f, .33f} : ac);
    r.text(257, 244, "RED: SPINNERS", 14, {1, .4f, .24f});
    r.text(257, 274, g.classic ? "FIRE: 1.5 / SEC" : "FIRE: 3.1 / SEC", 15, ac);
    r.text(257, 305, "PEAK " + number(g.peak), 16, muted);
    r.text(257, 332, "Y: AUTOFIRE " + std::string(g.autoFire ? "ON" : "OFF"), 14, ac);
  }
  r.rect(24, 371, 592, 1, {.13f, .22f, .29f});
  r.text(24, 385, a.music ? "NOW PLAYING" : "MUSIC MUTED / SELECT TO ENABLE", 13, ac);
  r.text(24, 405, Audio::titles[g.level - 1], 20);
  r.text(24, 435, Audio::artists[g.level - 1], 12, muted);
  r.text(527, 439, std::to_string(int(fps)) + " FPS", 12, muted);
  r.finish(false);
}
// One unobtrusive line of status, anchored to the physical screen edges.
static void compactHud(Renderer &r, const Game &g, int screen) {
  float scale = std::min(r.width / 640.f, r.height / 480.f);
  float w = r.width / scale, h = r.height / scale;
  r.overlay(w, h);
  const float size = 16, inset = 12;
  std::string score = screen == 0 ? "BEST " + number(g.high) : number(g.score);
  std::string cannons = "CANNONS " + std::to_string(screen == 0 ? 5 : g.lives);
  auto label = [&](float x, const std::string &text) {
    r.text(x + 1, inset + 1, text, size, {.005f, .01f, .015f, .85f});
    r.text(x, inset, text, size, {.93f, .97f, 1});
  };
  label(inset, score);
  label(w - inset - r.textWidth(cannons, size), cannons);
  r.finish(false);
}
static void upper(Renderer &r, Game &g, int screen, int selected, float time, bool single, Audio &a,
                  int pauseRow = 0) {
  Color ac = accents[g.level - 1];
  const float scale = std::min(r.width / 640.f, r.height / 480.f);
  const float w = r.width / scale, h = r.height / scale;
  const float left = (640 - w) / 2, top = (480 - h) / 2;
  auto shade = [&](Color color) {
    r.rect(left, top, w, h, color);
  };
  Color sky = skies[g.level - 1];
  if (g.dying()) {
    float weight = std::min(1.f, g.deathTimer / .4f);
    sky = {sky.r * (1 - weight) + .48f * weight, sky.g * (1 - weight) + .012f * weight,
           sky.b * (1 - weight) + .008f * weight};
  }
  r.begin(sky);
  r.world(g, g.menuSceneActive);
  r.finish();
  if (single)
    compactHud(r, g, screen);
  r.overlay();
  if (screen == 4 && g.menuSceneActive) {
    shade({.005f, .01f, .025f, .83f});
    r.text(320, 150, "Quit Game?", 36, {.96f, .98f, 1}, true);
    r.text(320, 255, "A QUIT     B CANCEL", 20, {.7f, .91f, 1}, true);
    r.finish(false);
    return;
  }
  if (screen == 0) {
    r.rect(left, top, w, 205 - top, {.004f, .01f, .02f, .24f});
    r.text(322, 55, "DOMEBLAST", 48, {.005f, .015f, .025f}, true);
    r.text(320, 53, "DOMEBLAST", 48, {.90f, .97f, 1}, true);
    r.text(320, 115, "3D", 32, {.36f, .90f, 1}, true);
    r.text(320, 167, "PLANETARY DEFENSE / SIX SKIES", 12, {.55f, .77f, .87f}, true);
    Color cyan{.36f, .9f, 1};
    r.rect(110, 382, 420, 39, {.014f, .055f, .083f, .91f});
    r.rect(110, 382, 3, 39, cyan);
    r.rect(113, 382, 417, 1, {.25f, .63f, .72f, .65f});
    r.text(320, 388,
           "A / START  " + std::string(selected ? "PRACTICE SECTOR " + std::to_string(selected)
                                                : "LAUNCH ARCADE"),
           19, {.92f, .98f, 1}, true);
    if (single)
      r.rect(110, 421, 420, 37, {.006f, .018f, .032f, .76f});
    r.text(320, single ? 424 : 432, "LEFT STICK MOVE   RIGHT STICK AIM   Y AUTOFIRE", 12, {.66f, .81f, .88f},
           true);
    r.text(320, single ? 443 : 455, "L2 / R2 CAMERA   START PAUSE   X EXIT", 11, {.43f, .62f, .71f}, true);
    if (single) {
      r.rect(110, 336, 420, 42, {.006f, .018f, .032f, .76f});
      r.text(320, 341, std::string("R3: ") + (g.classic ? "SLOW 1.5 SHOTS/SEC" : "STANDARD 3.1 SHOTS/SEC"), 11, cyan, true);
      r.text(320, 358, "D-PAD: SECTOR / VOLUME    SELECT: MUSIC", 11, cyan, true);
    }
  } else {
    if (g.banner > 0 && !g.over && !g.dying()) {
      r.text(320, 73, "SECTOR 0" + std::to_string(g.level), 16, ac, true);
      r.text(320, 100, names[g.level - 1], 24, {.96, .98, 1}, true);
    }
    if (!single)
      r.text(22, 448, g.practice ? "PRACTICE / BEST SCORE DISABLED" : "PEAK " + number(g.peak), 11,
           ac);
    if (screen == 2) {
      shade({.005f, .015f, .028f, .90f});
      r.text(320, 62, "PAUSED / AUDIO", 30, {.93f, .97f, 1}, true);
      r.text(320, 111, "UP / DOWN SELECT   LEFT / RIGHT ADJUST", 13, ac, true);
      const char *labels[] = {a.music ? "MUSIC" : "MUSIC (MUTED)", "SOUND EFFECTS", "MASTER"};
      int values[] = {a.musicVolume.load(), a.effectsVolume.load(), a.volume.load()};
      for (int row = 0; row < 3; row++) {
        float y = 157 + row * 62;
        if (row == pauseRow) {
          r.rect(65, y - 5, 510, 48, {.055f, .13f, .18f, .95f});
          r.rect(65, y - 5, 3, 48, ac);
        }
        r.text(83, y + 4, labels[row], 18, row == pauseRow ? ac : Color{.65f, .75f, .82f});
        r.rect(329, y + 15, 149, 5, {.15f, .24f, .29f});
        r.rect(329, y + 15, 149 * values[row] / 100.f, 5, ac);
        r.text(492, y + 3, std::to_string(values[row]) + "%", 19);
      }
      r.text(320, 370, "A / B / START RESUME     X TITLE", 17, {.85f, .94f, 1}, true);
      r.text(320, 412, "Y AUTOFIRE   SELECT MUSIC   SETTINGS ARE SAVED", 12, {.5f, .7f, .78f},
             true);
    } else if (screen == 3 || screen == 4) {
      shade({.004f, .01f, .025f, .78f});
      r.text(320, 140, screen == 4 ? "Return to Title?" : "SIGNAL LOST", 36, {.96f, .98f, 1}, true);
      r.text(320, 209, "PEAK SCORE  " + number(g.peak), 16, ac, true);
      r.text(320, 278, screen == 4 ? "A TITLE     B CANCEL" : "A RETRY     B TITLE", 18,
             {.84f, .92f, 1}, true);
    }
  }
  if (g.dying()) {
    float red = std::min(1.f, g.deathTimer / .4f);
    shade({.7f, .015f, .01f, .16f * red});
    r.text(320, 85, "CANNON LOST", 28, {1, .80f, .64f}, true);
    r.text(320, 122, g.lives > 0 ? "DEPLOYING RESERVE" : "FINAL CANNON", 15, {1, .60f, .42f}, true);
  }
  if (screen == 1 && !g.dying() && !single)
    r.text(320, 419, "Y  AUTO " + std::string(g.autoFire ? "ON" : "OFF"), 12, {.5f, .8f, .87f},
           true);
  r.finish(false);
  (void)time;
}
int main(int argc, char **argv) {
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  bool verify = false, single = false, demo = false, record = false, deathTest = false;
  int limit = 0;
  for (int i = 1; i < argc; i++) {
    std::string s = argv[i];
    if (s == "--verify-death") {
      verify = true;
      deathTest = true;
      limit = 8;
    }
    if (s == "--demo") {
      verify = true;
      demo = true;
      limit = 14;
    }
    if (s == "--record")
      record = true;
    if (s == "--verify")
      verify = true;
    if (s == "--single")
      single = true;
    if (s == "--seconds" && i + 1 < argc)
      limit = std::atoi(argv[++i]);
  }
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK |
               SDL_INIT_EVENTS) != 0) {
    std::fprintf(stderr, "SDL: %s\n", SDL_GetError());
    return 1;
  }
  SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
  SDL_ShowCursor(SDL_DISABLE);
  SDL_GameControllerAddMapping(
      "19009b4d4b4800000111000000010000,retrogame_joypad,a:b1,b:b0,x:b3,y:b2,"
      "back:b8,start:b9,guide:b10,leftshoulder:b4,rightshoulder:b5,lefttrigger:"
      "b6,righttrigger:b7,leftstick:b11,rightstick:b12,dpup:b13,dpdown:b14,"
      "dpleft:b15,dpright:b16,leftx:a0,lefty:a1,rightx:a2,righty:a3,platform:"
      "Linux,");
  if (const char *mappingFile = std::getenv("SDL_GAMECONTROLLERCONFIG_FILE"))
    SDL_GameControllerAddMappingsFromFile(mappingFile);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
  int displays = SDL_GetNumVideoDisplays(), top = 0, bottom = 1;
  for (int i = 0; i < displays; i++) {
    const char *n = SDL_GetDisplayName(i);
    std::printf("display %d %s\n", i, n ? n : "");
    if (!single && n && std::string(n).find("DSI-2") != std::string::npos)
      top = i;
    if (n && std::string(n).find("DSI-1") != std::string::npos)
      bottom = i;
  }
  if (top == bottom)
    bottom = top == 0 ? 1 : 0;
  bool dual = displays > 1 && !single;
  SDL_DisplayMode mode{};
  if (SDL_GetCurrentDisplayMode(top, &mode) != 0) {
    mode.w = 640;
    mode.h = 480;
  }
  SDL_Window *w = SDL_CreateWindow(
      "Domeblast 3D", SDL_WINDOWPOS_CENTERED_DISPLAY(top), SDL_WINDOWPOS_CENTERED_DISPLAY(top), mode.w,
      mode.h, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP);
  if (!w) {
    std::fprintf(stderr, "Window: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  SDL_GLContext ctx = SDL_GL_CreateContext(w);
  if (!ctx) {
    std::fprintf(stderr, "GL context: %s\n", SDL_GetError());
    SDL_DestroyWindow(w);
    SDL_Quit();
    return 1;
  }
  try {
    Renderer render;
    render.init();
    std::printf("GPU: %s / %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));
    SDL_GL_SetSwapInterval(0);
    SDL_Window *w2 = nullptr;
    SDL_GLContext ctx2 = nullptr;
    Renderer hud;
    if (dual) {
      w2 = SDL_CreateWindow("Domeblast 3D Secondary", SDL_WINDOWPOS_CENTERED_DISPLAY(bottom),
                            SDL_WINDOWPOS_CENTERED_DISPLAY(bottom), 640, 480,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP);
      if (w2)
        ctx2 = SDL_GL_CreateContext(w2);
      if (!ctx2) {
        dual = false;
        std::fprintf(stderr, "Secondary unavailable: %s\n", SDL_GetError());
      } else {
        hud.init();
        SDL_GL_SetSwapInterval(0);
      }
    }
    auto fitRenderer = [](SDL_Window *window, Renderer &renderer) {
      int width, height;
      SDL_GL_GetDrawableSize(window, &width, &height);
      if (width > 0 && height > 0)
        renderer.resize(width, height);
    };
    if (dual) {
      SDL_GL_MakeCurrent(w2, ctx2);
      fitRenderer(w2, hud);
    }
    SDL_GL_MakeCurrent(w, ctx);
    fitRenderer(w, render);
    viewportAspect = float(render.width) / render.height;
    std::printf("Layout: %s; drawable %dx%d; SDL driver %s\n",
                dual ? "dual" : "single", render.width, render.height, SDL_GetCurrentVideoDriver());
    auto showLoading = [&](int done, int total) {
      SDL_GL_MakeCurrent(w, ctx);
      render.begin({0, 0, 0}, false);
      render.overlay();
      render.picture(render.titleArt, 0, 0, 640, 480);
      render.rect(72, 383, 496, 63, {.005f, .025f, .04f, .88f});
      render.text(320, 392,
                  "PREPARING AUDIO  " + std::to_string(done) + " / " + std::to_string(total), 16,
                  {.75f, .93f, 1}, true);
      render.rect(108, 426, 424, 5, {.12f, .24f, .30f});
      render.rect(108, 426, 424 * done / float(total), 5, {.35f, .90f, 1});
      render.finish(false);
      if (verify && done == 0)
        render.capture("loading.ppm");
      if (verify && done == 3)
        render.capture("loading-progress.ppm");
      SDL_GL_SwapWindow(w);
      if (dual) {
        SDL_GL_MakeCurrent(w2, ctx2);
        hud.begin({.01f, .025f, .04f}, false);
        hud.overlay();
        hud.text(320, 115, "DOMEBLAST 3D", 30, {.75f, .94f, 1}, true);
        hud.text(320, 180, "PREPARING THE SIX SKIES SCORE", 16, {.45f, .72f, .84f}, true);
        hud.rect(95, 249, 450, 5, {.12f, .23f, .29f});
        hud.rect(95, 249, 450 * done / float(total), 5, {.35f, .9f, 1});
        hud.text(320, 288, std::to_string(done) + " / " + std::to_string(total) + " TRACKS READY",
                 15, {.65f, .86f, .95f}, true);
        hud.finish(false);
        SDL_GL_SwapWindow(w2);
      }
      SDL_PumpEvents();
    };
    showLoading(0, 6);
    Audio audio;
    if (record)
      audio.recording = std::fopen("video/audio.f32", "wb");
    if (!audio.init(showLoading))
      std::fprintf(stderr, "Audio unavailable: %s\n", SDL_GetError());
    else
      std::printf("Audio: stereo 44100 Hz; six recorded soundtrack tracks\n");
    Game game;
    int vol = 65, music = 1, classic = 0, autoFire = 1, musicVol = 80, effectsVol = 100;
    {
      std::ifstream f(savePath());
      f >> game.high >> vol >> music >> classic;
      f >> autoFire;
      f >> musicVol >> effectsVol;
    }
    audio.volume = clamp(vol, 0, 100);
    audio.music = music != 0;
    audio.musicVolume = int(clamp(musicVol, 0, 100));
    audio.effectsVolume = int(clamp(effectsVol, 0, 100));
    game.classic = classic != 0;
    game.autoFire = autoFire != 0;
    game.updateTitle(0, 0, 0);
    SDL_GameController *pad = nullptr;
    SDL_Joystick *joy = nullptr;
    auto openPad = [&]() {
      for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
          pad = SDL_GameControllerOpen(i);
          if (pad) {
            std::printf("Controller: %s %s\n", SDL_GameControllerName(pad),
                        SDL_GameControllerMapping(pad));
            break;
          }
        }
      }
      if (!pad && SDL_NumJoysticks()) {
        joy = SDL_JoystickOpen(0);
        std::printf("Raw joystick fallback: %s\n", SDL_JoystickName(joy));
      }
    };
    openPad();
    int screen = 0, priorScreen = 0, selected = 0, pauseRow = 0;
    bool running = true;
    float fps = 60, elapsed = 0, accum = 0;
    int frames = 0, verifyStage = -1;
    double last = SDL_GetPerformanceCounter() / double(SDL_GetPerformanceFrequency());
    auto begin = [&]() {
      game.reset(selected);
      screen = 1;
      audio.push(6);
    };
    while (running) {
      double start = SDL_GetPerformanceCounter() / double(SDL_GetPerformanceFrequency());
      SDL_GL_MakeCurrent(w, ctx);
      fitRenderer(w, render);
      viewportAspect = float(render.width) / render.height;
      float realdt = std::min(.10, double(start - last));
      last = start;
      elapsed += realdt;
      float dt = realdt;
      bool accept = false, back = false, pause = false, quit = false, toggleMusic = false,
           toggleMode = false, toggleAuto = false;
      int horizontal = 0, volumeChange = 0;
      SDL_Event e;
      while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT)
          running = false;
        // Steam overlay, desktop switching and suspend must not leave play active.
        if (!verify && screen == 1 &&
            ((e.type == SDL_WINDOWEVENT && e.window.windowID == SDL_GetWindowID(w) &&
              (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST ||
               e.window.event == SDL_WINDOWEVENT_MINIMIZED)) ||
             e.type == SDL_APP_WILLENTERBACKGROUND)) {
          screen = 2;
          save(game, audio);
        }
        if (e.type == SDL_CONTROLLERDEVICEADDED && !pad) {
          if (joy) {
            SDL_JoystickClose(joy);
            joy = nullptr;
          }
          openPad();
        }
        if (e.type == SDL_CONTROLLERDEVICEREMOVED && pad && !SDL_GameControllerGetAttached(pad)) {
          SDL_GameControllerClose(pad);
          pad = nullptr;
          if (screen == 1)
            screen = 2;
        }
        int b = -1;
        if (e.type == SDL_CONTROLLERBUTTONDOWN)
          b = e.cbutton.button;
        if (e.type == SDL_JOYBUTTONDOWN && !pad) {
          const int map[] = {SDL_CONTROLLER_BUTTON_B,
                             SDL_CONTROLLER_BUTTON_A,
                             SDL_CONTROLLER_BUTTON_Y,
                             SDL_CONTROLLER_BUTTON_X,
                             SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
                             SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
                             -1,
                             -1,
                             SDL_CONTROLLER_BUTTON_BACK,
                             SDL_CONTROLLER_BUTTON_START,
                             -1,
                             -1,
                             -1,
                             SDL_CONTROLLER_BUTTON_DPAD_UP,
                             SDL_CONTROLLER_BUTTON_DPAD_DOWN,
                             SDL_CONTROLLER_BUTTON_DPAD_LEFT,
                             SDL_CONTROLLER_BUTTON_DPAD_RIGHT};
          if (e.jbutton.button < 17)
            b = map[e.jbutton.button];
        }
        if (b == SDL_CONTROLLER_BUTTON_A)
          accept = true;
        if (b == SDL_CONTROLLER_BUTTON_B)
          back = true;
        if (b == SDL_CONTROLLER_BUTTON_START)
          pause = true;
        if (b == SDL_CONTROLLER_BUTTON_BACK)
          toggleMusic = true;
        if (b == SDL_CONTROLLER_BUTTON_X)
          quit = true;
        if (b == SDL_CONTROLLER_BUTTON_Y)
          toggleAuto = true;
        if (b == SDL_CONTROLLER_BUTTON_RIGHTSTICK)
          toggleMode = true;
        if (b == SDL_CONTROLLER_BUTTON_DPAD_LEFT)
          horizontal = -1;
        if (b == SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
          horizontal = 1;
        if (b == SDL_CONTROLLER_BUTTON_DPAD_UP)
          volumeChange = 5;
        if (b == SDL_CONTROLLER_BUTTON_DPAD_DOWN)
          volumeChange = -5;
        if (e.type == SDL_KEYDOWN && !e.key.repeat) {
          switch (e.key.keysym.sym) {
          case SDLK_RETURN:
            accept = true;
            break;
          case SDLK_ESCAPE:
            if (screen == 1)
              pause = true;
            else
              quit = true;
            break;
          case SDLK_p:
            pause = true;
            break;
          case SDLK_b:
            back = true;
            break;
          case SDLK_m:
            toggleMusic = true;
            break;
          case SDLK_x:
            quit = true;
            break;
          case SDLK_f:
            toggleAuto = true;
            break;
          case SDLK_c:
            toggleMode = true;
            break;
          case SDLK_LEFT:
            horizontal = -1;
            break;
          case SDLK_RIGHT:
            horizontal = 1;
            break;
          case SDLK_UP:
            volumeChange = 5;
            break;
          case SDLK_DOWN:
            volumeChange = -5;
            break;
          default:
            break;
          }
        }
      }
      if (toggleAuto) {
        game.autoFire = !game.autoFire;
        std::printf("AUTOFIRE %d\n", game.autoFire);
        if (!verify)
          save(game, audio);
      }
      if (toggleMusic)
        audio.music = !audio.music;
      if (screen == 2) {
        if (volumeChange)
          pauseRow = (pauseRow + (volumeChange > 0 ? -1 : 1) + 3) % 3;
        if (horizontal) {
          int delta = horizontal * 5;
          if (pauseRow == 0) {
            audio.musicVolume = int(clamp(audio.musicVolume + delta, 0, 100));
            audio.music = audio.musicVolume > 0;
          }
          if (pauseRow == 1) {
            audio.effectsVolume = int(clamp(audio.effectsVolume + delta, 0, 100));
            audio.push(1);
          }
          if (pauseRow == 2)
            audio.volume = int(clamp(audio.volume + delta, 0, 100));
          if (!verify)
            save(game, audio);
          std::printf("AUDIO SETTINGS music=%d effects=%d master=%d\n", audio.musicVolume.load(),
                      audio.effectsVolume.load(), audio.volume.load());
        }
      } else if (volumeChange && screen == 0)
        audio.volume = int(clamp(audio.volume + volumeChange, 0, 100));
      if (toggleMode && screen == 0)
        game.classic = !game.classic;
      if (quit) {
        if (screen != 4) {
          priorScreen = screen;
          screen = 4;
        }
      } else if (screen == 4) {
        if (back || pause)
          screen = priorScreen;
        else if (accept) {
          if (priorScreen == 0)
            running = false;
          else {
            if (!verify)
              save(game, audio);
            screen = 0;
          }
        }
      } else if (screen == 0) {
        if (horizontal)
          selected = (selected + horizontal + 7) % 7;
        if (accept || pause)
          begin();
      } else if (screen == 1) {
        if (pause)
          screen = 2;
      } else if (screen == 2) {
        if (accept || back || pause)
          screen = 1;
      } else if (screen == 3) {
        if (accept || pause)
          begin();
        if (back)
          screen = 0;
      }
      Input in;
      const Uint8 *keys = SDL_GetKeyboardState(nullptr);
      in.move = (keys[SDL_SCANCODE_D] ? 1.f : 0) - (keys[SDL_SCANCODE_A] ? 1.f : 0);
      in.movey = (keys[SDL_SCANCODE_S] ? 1.f : 0) - (keys[SDL_SCANCODE_W] ? 1.f : 0);
      in.orbit = (keys[SDL_SCANCODE_E] ? 1.f : 0) - (keys[SDL_SCANCODE_Q] ? 1.f : 0);
      in.aimx = (keys[SDL_SCANCODE_L] ? 1.f : 0) - (keys[SDL_SCANCODE_J] ? 1.f : 0);
      in.aimy = keys[SDL_SCANCODE_I] ? -1 : 0;
      in.fire = keys[SDL_SCANCODE_K];
      in.focus = keys[SDL_SCANCODE_LSHIFT];
      auto dead = [](float x) {
        return std::abs(x) < .18f ? 0.f : (x - std::copysign(.18f, x)) / .82f;
      };
      if (pad) {
        in.move = dead(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX) / 32767.f);
        in.movey = dead(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY) / 32767.f);
        in.focus = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSTICK);
        in.orbit =
            (SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 10000 ? 1.f : 0) -
            (SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 10000 ? 1.f : 0);
        in.aimx = dead(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX) / 32767.f);
        in.aimy = dead(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY) / 32767.f);
        in.fire = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) ||
                  SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A);
        if (!in.movey)
          in.movey = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) -
                     SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP);
        if (!in.move)
          in.move = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) -
                    SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
      } else if (joy) {
        in.move = dead(SDL_JoystickGetAxis(joy, 0) / 32767.f);
        in.movey = dead(SDL_JoystickGetAxis(joy, 1) / 32767.f);
        in.orbit = SDL_JoystickGetButton(joy, 7) - SDL_JoystickGetButton(joy, 6);
        in.aimx = dead(SDL_JoystickGetAxis(joy, 2) / 32767.f);
        in.aimy = dead(SDL_JoystickGetAxis(joy, 3) / 32767.f);
        in.fire = SDL_JoystickGetButton(joy, 5) || SDL_JoystickGetButton(joy, 1);
      }
      if (verify) {
        int stage = demo || deathTest ? 4 : int(elapsed / 5);
        if (stage >= 7)
          running = false;
        else if (stage != verifyStage) {
          verifyStage = stage;
          if (stage == 0) {
            screen = 0;
            selected = 0;
          } else {
            selected = stage;
            begin();
            for (int i = 0; i < 12; i++) {
              auto en = game.make(Kind(i % 5), -6 + (i % 6) * 2, 3 + (i / 6) * 5);
              en.p.z = -5 + (i % 4) * 3.1f;
              game.enemies.push_back(en);
            }
            if (stage >= 4) {
              auto en = game.make(Ufo, 2, 10);
              en.v = {1, 0, .3f};
              en.p.z = -3;
              game.enemies.push_back(en);
            }
          }
        }
        in.move = std::sin(elapsed * 1.3f);
        in.movey = std::cos(elapsed * .9f) * .65f;
        in.orbit = .22f;
        in.aimx = std::sin(elapsed * .8f) * .6f;
        in.aimy = -1;
        in.fire = true;
        if (!deathTest) {
          game.invincible = true;
          game.lives = 5;
        }
      }
      if (deathTest) {
        in = {};
        game.invincible = false;
        game.autoFire = false;
        static int trigger = 0;
        if (trigger == 0) {
          game.enemies.clear();
          game.lives = 3;
          trigger = 1;
        }
        if (trigger == 1 && elapsed > 1) {
          auto b = game.make(Spinner, -5, .001f);
          b.p.z = -4;
          game.enemies.push_back(b);
          trigger = 2;
        }
        if (trigger == 2 && elapsed > 3.5f) {
          auto b = game.make(Rock, game.x, .5f);
          b.p.z = game.z;
          game.enemies.push_back(b);
          trigger = 3;
        }
        if (trigger == 3 && elapsed > 6) {
          game.lives = 1;
          auto b = game.make(Missile, game.x, .5f);
          b.p.z = game.z;
          game.enemies.push_back(b);
          trigger = 4;
        }
      }
      game.syncTerrain(audio.terrainTimeline.load(), realdt, audio.device != 0);
      if (screen == 1) {
        int steps = std::max(1, int(std::ceil(dt / (1.f / 60))));
        for (int step = 0; step < steps; ++step)
          game.update(dt / steps, in);
        if (game.over) {
          screen = 3;
          if (!verify)
            save(game, audio);
        }
      } else if (screen == 0 || (screen == 4 && game.menuSceneActive)) {
        game.updateTitle(elapsed, dt, selected);
      }
      audio.level = game.level;
      audio.shipDucking = game.spaceshipDucking();
      audio.active = screen == 1 && !game.dying();
      audio.sceneVisible = screen == 1;
      audio.viewTerrain = game.terrain.packed();
      audio.viewAngle = game.cameraAngle;
      audio.viewShake = cameraShake(game);
      auto threatAudio = visibleThreatAudio(game);
      audio.whistle = threatAudio.whistle;
      audio.missileThreat = threatAudio.missile;
      audio.ufoPresent = threatAudio.ufo;
      for (const auto &s : game.sounds) {
        audio.push(s);
      }
      static int reportedScreen = -1;
      if (screen != reportedScreen) {
        std::printf("STATE %d (0=title 1=play 2=pause 3=gameover 4=exit)\n", screen);
        reportedScreen = screen;
      }
      game.sounds.clear();
      SDL_GL_MakeCurrent(w, ctx);
      upper(render, game, screen, selected, elapsed, !dual, audio, pauseRow);

      static int recordedFrame = -1, previousVideoTick = -1;
      int videoFrame = int(elapsed * 12);
      if (record && videoFrame != previousVideoTick) {
        previousVideoTick = videoFrame;
        char name[80];
        std::snprintf(name, sizeof(name), "video/frame-%05d.ppm", ++recordedFrame);
        render.capture(name);
      }
      if (deathTest) {
        static int shot = 0;
        const float times[] = {.7f, 1.15f, 1.8f, 2.4f, 2.7f, 4.2f, 5.2f, 6.8f, 7.8f};
        if (shot < 9 && elapsed >= times[shot]) {
          std::string p = "death-" + std::to_string(shot) + ".ppm";
          render.capture(p.c_str());
          std::printf("DEATHCHECK %d lives=%d dying=%d over=%d timer=%.2f enemies=%zu bullets=%zu "
                      "particles=%zu blasts=%zu\n",
                      shot, game.lives, game.dying(), game.over, game.deathTimer,
                      game.enemies.size(), game.bullets.size(), game.particles.size(),
                      game.blasts.size());
          shot++;
        }
      }
      static int captured = -1;
      if (verify && elapsed - verifyStage * 5 > 3 && captured != verifyStage) {
        std::string file = "capture-" + std::to_string(verifyStage) + ".ppm";
        render.capture(file.c_str());
        captured = verifyStage;
        std::printf("VERIFY sector=%d score=%d enemies=%zu GL_error=%u fps=%.1f\n", verifyStage,
                    game.score, game.enemies.size(), glGetError(), fps);
        std::fflush(stdout);
      }
      SDL_GL_SwapWindow(w);
      if (dual) {
        SDL_GL_MakeCurrent(w2, ctx2);
        fitRenderer(w2, hud);
        panel(hud, game, screen, selected, audio, fps);
        if (verify && elapsed > 33 && elapsed < 33.1)
          hud.capture("capture-hud.ppm");
        SDL_GL_SwapWindow(w2);
      }
      frames++;
      accum += realdt;
      if (accum >= 2) {
        fps = frames / accum;
        std::printf("FPS %.1f sector %d objects %zu position=(%.2f,%.2f) camera=%.2f "
                    "aim=(%.2f,%.2f,%.2f)\n",
                    fps, game.level, game.enemies.size(), game.x, game.z, game.cameraAngle,
                    game.aimDir.x, game.aimDir.y, game.aimDir.z);
        std::fflush(stdout);
        frames = 0;
        accum = 0;
      }
      if (limit && elapsed >= limit)
        running = false;
      double used = SDL_GetPerformanceCounter() / double(SDL_GetPerformanceFrequency()) - start;
      if (used < 1. / 60)
        SDL_Delay(Uint32((1. / 60 - used) * 1000));
    }
    if (!verify)
      save(game, audio);
    if (pad)
      SDL_GameControllerClose(pad);
    if (joy)
      SDL_JoystickClose(joy);
    if (ctx2)
      SDL_GL_DeleteContext(ctx2);
    if (w2)
      SDL_DestroyWindow(w2);
  } catch (const std::exception &e) {
    std::fprintf(stderr, "Fatal: %s\n", e.what());
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(w);
    SDL_Quit();
    return 1;
  }
  SDL_GL_DeleteContext(ctx);
  SDL_DestroyWindow(w);
  SDL_Quit();
  return 0;
}
