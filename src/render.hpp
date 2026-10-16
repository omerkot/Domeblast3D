#pragma once
#include "game.hpp"
#include "geometry.hpp"
#include "material_shaders.hpp"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "../third_party/stb_image.h"
#pragma GCC diagnostic pop
#include <GLES2/gl2.h>
#include <SDL.h>
#include <cstdio>
#include <stdexcept>
namespace db {
struct Vertex {
  float x, y, z, r, g, b, a, u, v, nx = 0, ny = 0, nz = 0, mat = 0;
};
struct Renderer {
  GLuint titleArt = 0, atlas = 0, prog = 0, post = 0, vbo = 0, font = 0, white = 0, fbo = 0,
         scene = 0, depth = 0;
  GLuint targetMask = 0, targetFbo = 0, maskProgram = 0, haloProgram = 0;
  GLuint colorProg = 0, colorPost = 0;
  int colorPipeline = 0; // 0: original, 1: managed RGBA8, 2: managed linear RGBA16F
  std::vector<Vertex> targetMesh;
  bool haloReady = false;
  float haloStrength = 0;
  int haloX = 0, haloY = 0, haloWidth = 0, haloHeight = 0;
  std::vector<Vertex> verts, sandMesh;
  std::array<std::vector<Vertex>, 6> sectorSandMeshes;
  std::array<float, 6> sandWeights{};
  Terrain terrain;
  float terrainHeight(float x, float z) const { return terrain.height(x, z); }
  V terrainNormal(float x, float z) const { return terrain.normal(x, z); }
  float matrix[16]{};
  int material = 2;
  V uvOrigin{}, eyePosition{}, impactPosition{};
  Color fogColor{};
  float impactPower = 0, targetGlow = 0;
  V viewRight{1, 0, 0}, viewUp{0, 1, 0}, eyeDir{0, 0, 1};
  float clock = 0;
  bool ui = false;
  bool useTrackedView = false;
  CameraView trackedView{0};
  int width = 640, height = 480;
  static GLuint shader(GLenum type, const char *s) {
    GLuint n = glCreateShader(type);
    glShaderSource(n, 1, &s, nullptr);
    glCompileShader(n);
    GLint ok;
    glGetShaderiv(n, GL_COMPILE_STATUS, &ok);
    if (!ok) {
      char log[4096];
      glGetShaderInfoLog(n, sizeof(log), nullptr, log);
      throw std::runtime_error(log);
    }
    return n;
  }
  static GLuint program(const char *v, const char *f) {
    GLuint p = glCreateProgram(), vs = shader(GL_VERTEX_SHADER, v),
           fs = shader(GL_FRAGMENT_SHADER, f);
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glBindAttribLocation(p, 0, "pos");
    glBindAttribLocation(p, 1, "color");
    glBindAttribLocation(p, 2, "uv");
    glBindAttribLocation(p, 3, "normal");
    glBindAttribLocation(p, 4, "material");
    glLinkProgram(p);
    GLint ok;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok)
      throw std::runtime_error("shader link failed");
    glDeleteShader(vs);
    glDeleteShader(fs);
    return p;
  }
  void init() {
    prog = program(materialVS, materialFS);
    post = program("attribute vec3 pos;attribute vec2 uv;varying vec2 t;void "
                   "main(){gl_Position=vec4(pos,1.);t=uv;}",
                   finishFS);
    maskProgram = program(materialVS, targetMaskFS);
    haloProgram = program("attribute vec3 pos;attribute vec2 uv;varying vec2 t;void "
                          "main(){gl_Position=vec4(pos,1.);t=uv;}",
                          targetHaloFS);
    int tw, th, tc;
    unsigned char *pixels = stbi_load("assets/textures/material-atlas.png", &tw, &th, &tc, 4);
    if (!pixels)
      throw std::runtime_error("Cannot load material atlas");
    glGenTextures(1, &atlas);
    glBindTexture(GL_TEXTURE_2D, atlas);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(pixels);
    pixels = stbi_load("assets/title/domeblast-3d.png", &tw, &th, &tc, 4);
    if (!pixels)
      throw std::runtime_error("Cannot load Domeblast 3D title artwork");
    glGenTextures(1, &titleArt);
    glBindTexture(GL_TEXTURE_2D, titleArt);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(pixels);
    glGenBuffers(1, &vbo);
    glGenTextures(1, &white);
    glBindTexture(GL_TEXTURE_2D, white);
    unsigned char pixel[] = {255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    SDL_Surface *bmp = SDL_LoadBMP("assets/font.bmp");
    if (!bmp)
      throw std::runtime_error(SDL_GetError());
    SDL_Surface *rgba = SDL_ConvertSurfaceFormat(bmp, SDL_PIXELFORMAT_ABGR8888, 0);
    glGenTextures(1, &font);
    glBindTexture(GL_TEXTURE_2D, font);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 rgba->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    SDL_FreeSurface(rgba);
    SDL_FreeSurface(bmp);
    glGenTextures(1, &scene);
    glBindTexture(GL_TEXTURE_2D, scene);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, scene, 0);
    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
      throw std::runtime_error("incomplete framebuffer");
    glGenTextures(1, &targetMask);
    glBindTexture(GL_TEXTURE_2D, targetMask);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &targetFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, targetMask, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
      throw std::runtime_error("incomplete target mask framebuffer");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    verts.reserve(150000);
  }
  void allocateScene() {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scene);
    // GLES3/WebGL2 enums; this format is requested only after capability detection.
    glTexImage2D(GL_TEXTURE_2D, 0, colorPipeline == 2 ? 0x881A /* RGBA16F */ : GL_RGBA,
                 width, height, 0, GL_RGBA, colorPipeline == 2 ? 0x140B /* HALF_FLOAT */ : GL_UNSIGNED_BYTE,
                 nullptr);
  }
  int setColorPipeline(int requested) {
    int next = int(clamp(requested, 0, 2));
    if (next == colorPipeline) return colorPipeline;
    flush();
    if (next && !colorProg) {
      std::string materialSource = std::string("#define DB_MANAGED_COLOR\n") + materialFS;
      std::string finishSource = std::string("#define DB_MANAGED_COLOR\n") + finishFS;
      colorProg = program(materialVS, materialSource.c_str());
      colorPost = program("attribute vec3 pos;attribute vec2 uv;varying vec2 t;void "
                          "main(){gl_Position=vec4(pos,1.);t=uv;}", finishSource.c_str());
    }
    bool reallocate = (next == 2) != (colorPipeline == 2);
    colorPipeline = next;
    if (reallocate) {
      allocateScene();
      GLenum error = glGetError();
      glBindFramebuffer(GL_FRAMEBUFFER, fbo);
      if (error != GL_NO_ERROR || glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        colorPipeline = 1;
        allocateScene();
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
          throw std::runtime_error("Cannot allocate scene color buffer");
      }
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    return colorPipeline;
  }
  void resize(int w, int h) {
    if (w == width && h == height)
      return;
    width = std::max(1, w);
    height = std::max(1, h);
    viewportAspect = float(width) / height;
    allocateScene();
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
    glBindTexture(GL_TEXTURE_2D, targetMask);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  }
  void identity() {
    for (int i = 0; i < 16; i++)
      matrix[i] = i % 5 == 0 ? 1 : 0;
  }
  void camera(float angle = .45f, float motion = 0) {
    if (useTrackedView) {
      std::copy(trackedView.clip.begin(), trackedView.clip.end(), matrix);
      eyePosition = trackedView.eye;
      viewRight = trackedView.right;
      viewUp = trackedView.up;
      eyeDir = trackedView.forward * -1;
      return;
    }
    CameraView cameraFrame(angle, motion);
    V eye = cameraFrame.eye, f = cameraFrame.forward, s = cameraFrame.right, u = cameraFrame.up;
    eyePosition = eye;
    viewRight = s;
    viewUp = u;
    eyeDir = f * -1;
    float view[] = {s.x, u.x, -f.x, 0, s.y,          u.y,          -f.y,        0,
                    s.z, u.z, -f.z, 0, -dot(s, eye), -dot(u, eye), dot(f, eye), 1};
    float q = 1 / cameraVerticalTangent();
    float proj[16] = {q / viewportAspect, 0, 0, 0, 0, q, 0, 0, 0, 0, -1.004f, -1, 0, 0, -.2004f, 0};
    for (int c = 0; c < 4; c++)
      for (int r = 0; r < 4; r++) {
        matrix[c * 4 + r] = 0;
        for (int k = 0; k < 4; k++)
          matrix[c * 4 + r] += proj[k * 4 + r] * view[c * 4 + k];
      }
  }
  void overlay(float logicalWidth = 640, float logicalHeight = 480) {
    flush();
    ui = true;
    identity();
    float scale = std::min(width / logicalWidth, height / logicalHeight);
    matrix[0] = 2.f * scale / width;
    matrix[5] = -2.f * scale / height;
    matrix[12] = -logicalWidth * scale / width;
    matrix[13] = logicalHeight * scale / height;
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  }
  void vertex(V p, Color c, float u = 0, float v = 0, V normal = {}, float mat = 0) {
    verts.push_back({p.x, p.y, p.z, c.r, c.g, c.b, c.a, u, v, normal.x, normal.y, normal.z, mat});
  }
  void tri(V a, V b, V c, Color col, bool lit = true) {
    V n = norm(cross(b - a, c - a));
    auto put = [&](V p) {
      V q = p - uvOrigin;
      float u, v;
      if (std::abs(n.y) > .7f) {
        u = q.x * .23f;
        v = q.z * .23f;
      } else if (std::abs(n.x) > std::abs(n.z)) {
        u = q.z * .45f;
        v = q.y * .45f;
      } else {
        u = q.x * .45f;
        v = q.y * .45f;
      }
      vertex(p, col, lit ? u : 0, lit ? v : 0, n, lit ? material : 0);
    };
    put(a);
    put(b);
    put(c);
  }
  void box(V p, V size, Color c, float angle = 0) {
    V savedOrigin = uvOrigin;
    uvOrigin = p;
    V v[8];
    for (int i = 0; i < 8; i++) {
      float x = (i & 1 ? .5f : -.5f) * size.x, y = (i & 2 ? .5f : -.5f) * size.y;
      v[i] = p + V{x * std::cos(angle) - y * std::sin(angle),
                   x * std::sin(angle) + y * std::cos(angle), (i & 4 ? .5f : -.5f) * size.z};
    }
    const int idx[] = {0, 2, 3, 0, 3, 1, 4, 5, 7, 4, 7, 6, 0, 4, 6, 0, 6, 2,
                       1, 3, 7, 1, 7, 5, 2, 6, 7, 2, 7, 3, 0, 1, 5, 0, 5, 4};
    for (int i = 0; i < 36; i += 3)
      tri(v[idx[i]], v[idx[i + 1]], v[idx[i + 2]], c);
    uvOrigin = savedOrigin;
  }
  void rock(V p, float radius, float angle, Color c, uint32_t seed, int rings = 9, int segs = 14,
            V scale = {1, 1, 1}, int surface = 3) {
    auto point = [&](int r, int s) {
      float a = PI * r / rings, b = 2 * PI * (s % segs) / segs;
      float wob = surface == 2 || surface >= 5
                      ? 1.f
                      : 1 + .10f * std::sin(float((r * 31 + s % segs * 17 + seed % 133) * 71));
      V local{std::sin(a) * std::cos(b) * radius * wob, std::cos(a) * radius * wob,
              std::sin(a) * std::sin(b) * radius * wob};
      return V{(local.x * std::cos(angle) - local.y * std::sin(angle)) * scale.x,
               (local.x * std::sin(angle) + local.y * std::cos(angle)) * scale.y,
               local.z * scale.z};
    };
    auto put = [&](int r, int s) {
      V v = point(r, s);
      V n =
          norm(V{v.x / (scale.x * scale.x), v.y / (scale.y * scale.y), v.z / (scale.z * scale.z)});
      vertex(p + v, c, s / float(segs) * 2, r / float(rings), n, surface);
    };
    for (int r = 0; r < rings; r++)
      for (int s = 0; s < segs; s++) {
        put(r, s);
        put(r + 1, s + 1);
        put(r + 1, s);
        put(r, s);
        put(r, s + 1);
        put(r + 1, s + 1);
      }
  }
  void ring(V p, float r, float thick, Color c, float rot = 0, int segments = 40) {
    for (int i = 0; i < segments; i++) {
      float a = i * 2 * PI / segments, b = (i + 1) * 2 * PI / segments;
      auto pt = [&](float angle, float radius) {
        return p + V{std::cos(angle) * radius, std::sin(angle) * radius * std::cos(rot),
                     std::sin(angle) * radius * std::sin(rot)};
      };
      tri(pt(a, r), pt(b, r), pt(b, r + thick), c, false);
      tri(pt(a, r), pt(b, r + thick), pt(a, r + thick), c, false);
    }
  }
  void line(V a, V b, float w, Color c) {
    V n = norm(cross(b - a, eyeDir)) * w;
    tri(a - n, b - n, b + n, c, false);
    tri(a - n, b + n, a + n, c, false);
  }
  void rect(float x, float y, float w, float h, Color c) {
    tri({x, y, 0}, {x + w, y, 0}, {x + w, y + h, 0}, c, false);
    tri({x, y, 0}, {x + w, y + h, 0}, {x, y + h, 0}, c, false);
  }
  void flush(GLuint texture = 0, bool isText = false, bool displaySpace = false) {
    if (verts.empty())
      return;
    GLuint activeProgram = colorPipeline && !displaySpace ? colorProg : prog;
    glUseProgram(activeProgram);
    glUniform1f(glGetUniformLocation(activeProgram, "linearScene"), colorPipeline == 2 ? 1.f : 0.f);
    glUniformMatrix4fv(glGetUniformLocation(activeProgram, "m"), 1, GL_FALSE, matrix);
    glUniform1f(glGetUniformLocation(activeProgram, "textMode"), isText ? 1 : 0);
    glUniform1f(glGetUniformLocation(activeProgram, "targetGlow"), targetGlow);
    glUniform3f(glGetUniformLocation(activeProgram, "eye"), eyePosition.x, eyePosition.y, eyePosition.z);
    glUniform3f(glGetUniformLocation(activeProgram, "fogColor"), fogColor.r, fogColor.g, fogColor.b);
    glUniform4f(glGetUniformLocation(activeProgram, "impact"), impactPosition.x, impactPosition.y,
                impactPosition.z, impactPower);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, atlas);
    glUniform1i(glGetUniformLocation(activeProgram, "atlas"), 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture ? texture : white);
    glUniform1i(glGetUniformLocation(activeProgram, "tex"), 0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STREAM_DRAW);
    attributes();
    glDrawArrays(GL_TRIANGLES, 0, verts.size());
    verts.clear();
  }
  void setTargetGlow(float strength) {
    if (strength == targetGlow)
      return;
    // Keep the highlight confined to this entity's draw calls.
    flush();
    targetGlow = strength;
  }
  void attributes() {
    for (int i = 0; i < 5; i++)
      glEnableVertexAttribArray(i);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)(3 * sizeof(float)));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)(7 * sizeof(float)));
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)(9 * sizeof(float)));
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)(12 * sizeof(float)));
  }
  void picture(GLuint texture, float x, float y, float width, float height) {
    flush();
    Color whiteColor{1, 1, 1};
    vertex({x, y, 0}, whiteColor, 0, 0);
    vertex({x + width, y, 0}, whiteColor, 1, 0);
    vertex({x + width, y + height, 0}, whiteColor, 1, 1);
    vertex({x, y, 0}, whiteColor, 0, 0);
    vertex({x + width, y + height, 0}, whiteColor, 1, 1);
    vertex({x, y + height, 0}, whiteColor, 0, 1);
    flush(texture);
  }
  float textWidth(const std::string &s, float size) { return s.size() * size * .57f; }
  void text(float x, float y, const std::string &s, float size, Color c = {.88, .94, 1},
            bool center = false) {
    flush();
    if (center)
      x -= textWidth(s, size) / 2;
    float h = size * 1.55f;
    for (unsigned char ch : s) {
      if (ch < 32 || ch > 127)
        ch = '?';
      int n = ch - 32;
      float u = (n % 16) / 16.f, v = (n / 16) / 6.f;
      vertex({x, y, 0}, c, u, v);
      vertex({x + h, y, 0}, c, u + 1.f / 16, v);
      vertex({x + h, y + h, 0}, c, u + 1.f / 16, v + 1.f / 6);
      vertex({x, y, 0}, c, u, v);
      vertex({x + h, y + h, 0}, c, u + 1.f / 16, v + 1.f / 6);
      vertex({x, y + h, 0}, c, u, v + 1.f / 6);
      x += size * .57f;
    }
    flush(font, true);
  }
  void begin(Color sky, bool effects = true) {
    ui = false;
    fogColor = sky;
    material = 2;
    impactPower = 0;
    targetGlow = 0;
    haloReady = false;
    haloStrength = 0;
    targetMesh.clear();
    glDepthMask(GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER, effects ? fbo : 0);
    glViewport(0, 0, width, height);
    if (colorPipeline == 2) {
      auto decode = [](float c) { return c <= .04045f ? c / 12.92f : std::pow((c + .055f) / 1.055f, 2.4f); };
      glClearColor(decode(sky.r), decode(sky.g), decode(sky.b), 1);
    } else glClearColor(sky.r, sky.g, sky.b, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    camera();
  }
  void finish(bool effects = true, GLuint outputFramebuffer = 0, int outputX = 0, int outputY = 0,
              int outputWidth = 0, int outputHeight = 0) {
    flush();
    if (effects) {
      glBindFramebuffer(GL_FRAMEBUFFER, outputFramebuffer);
      int ow = outputWidth ? outputWidth : width, oh = outputHeight ? outputHeight : height;
      glViewport(outputX, outputY, ow, oh);
      glDisable(GL_DEPTH_TEST);
      glDisable(GL_BLEND);
      verts = {{-1, -1, 0, 1, 1, 1, 1, 0, 0}, {1, -1, 0, 1, 1, 1, 1, 1, 0},
               {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, -1, 0, 1, 1, 1, 1, 0, 0},
               {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, 1, 0, 1, 1, 1, 1, 0, 1}};
      GLuint finishProgram = colorPipeline ? colorPost : post;
      glUseProgram(finishProgram);
      glUniform1f(glGetUniformLocation(finishProgram, "linearScene"), colorPipeline == 2 ? 1.f : 0.f);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, scene);
      glUniform1i(glGetUniformLocation(finishProgram, "tex"), 0);
      glUniform2f(glGetUniformLocation(finishProgram, "pixelSize"), 1.f / width, 1.f / height);
      glUniform1f(glGetUniformLocation(finishProgram, "vignetteStrength"), useTrackedView ? 0.f : 1.f);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);
      glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STREAM_DRAW);
      attributes();
      glDrawArrays(GL_TRIANGLES, 0, 6);
      if (haloReady) {
        // Restrict the outline pass to the small target rectangle on the RGDS.
        glEnable(GL_SCISSOR_TEST);
        // The two XR eyes can occupy different rectangles in one compositor framebuffer.
        int x0 = outputX + int(std::floor(float(haloX) * ow / width));
        int y0 = outputY + int(std::floor(float(haloY) * oh / height));
        int x1 = outputX + int(std::ceil(float(haloX + haloWidth) * ow / width));
        int y1 = outputY + int(std::ceil(float(haloY + haloHeight) * oh / height));
        glScissor(x0, y0, x1 - x0, y1 - y0);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(haloProgram);
        glBindTexture(GL_TEXTURE_2D, targetMask);
        glUniform1i(glGetUniformLocation(haloProgram, "maskTex"), 0);
        glUniform2f(glGetUniformLocation(haloProgram, "pixelSize"), 1.f / width, 1.f / height);
        glUniform1f(glGetUniformLocation(haloProgram, "radiusScale"),
                    useTrackedView ? std::max(1.f, height / 1536.f) : 3.f * height / 480.f);
        glUniform1f(glGetUniformLocation(haloProgram, "vrHalo"), useTrackedView ? 1.f : 0.f);
        glUniform1f(glGetUniformLocation(haloProgram, "strength"), haloStrength);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glDisable(GL_BLEND);
        glDisable(GL_SCISSOR_TEST);
      }
      verts.clear();
    }
  }
  void armoredHull(V p, float width, float length, float height, float yaw, Color c,
                   V surfaceUp = {0, 1, 0}) {
    V facing{std::sin(yaw), 0, std::cos(yaw)};
    facing = norm(facing - surfaceUp * dot(facing, surfaceUp));
    V side = norm(cross(surfaceUp, facing));
    auto orient = [&](V local) { return side * local.x + surfaceUp * local.y + facing * local.z; };
    V outline[6] = {{0, 0, -length * .58f},           {-width * .36f, 0, -length * .24f},
                    {-width * .5f, 0, length * .35f}, {0, 0, length * .5f},
                    {width * .5f, 0, length * .35f},  {width * .36f, 0, -length * .24f}};
    V lo[6], hi[6];
    V old = uvOrigin;
    uvOrigin = p;
    for (int i = 0; i < 6; i++) {
      lo[i] = p + orient(outline[i]);
      hi[i] = p + orient(outline[i] * .72f + V{0, height, 0});
    }
    for (int i = 0; i < 6; i++) {
      int j = (i + 1) % 6;
      tri(lo[i], hi[j], hi[i], c);
      tri(lo[i], lo[j], hi[j], c);
      tri(p + surfaceUp * height, hi[i], hi[j], c);
      tri(p, lo[j], lo[i], c);
    }
    uvOrigin = old;
  }
  void asteroid(V p, float radius, float angle, Color c, uint32_t seed) {
    constexpr int rings = 7, segs = 11;
    auto point = [&](int r, int j) {
      float a = PI * r / rings, b = 2 * PI * j / segs;
      V u{std::sin(a) * std::cos(b), std::cos(a), std::sin(a) * std::sin(b)};
      return rotateXYZ(asteroidShape(u, seed) * radius, angle * .43f, angle * .7f, angle);
    };
    auto face = [&](int ar, int as, int br, int bs, int cr, int cs) {
      V a = point(ar, as), b = point(br, bs), d = point(cr, cs);
      V n = norm(cross(b - a, d - a));
      int mat = seed % 4 == 0 ? 8 : 7;
      vertex(p + a, c, as / float(segs) * 2, ar / float(rings), norm(n * .8f + norm(a) * .2f), mat);
      vertex(p + b, c, bs / float(segs) * 2, br / float(rings), norm(n * .8f + norm(b) * .2f), mat);
      vertex(p + d, c, cs / float(segs) * 2, cr / float(rings), norm(n * .8f + norm(d) * .2f), mat);
    };
    for (int r = 0; r < rings; r++)
      for (int j = 0; j < segs; j++) {
        face(r, j, r + 1, j + 1, r + 1, j);
        face(r, j, r, j + 1, r + 1, j + 1);
      }
  }
  void sphericalBomb(const Entity &e) {
    auto spin = [&](V p) { return rotateXYZ(p, e.angle * .71f, e.angle * .53f, e.angle); };
    rock(e.p, e.r * .48f, 0, {.94f, .95f, 1}, 0, 6, 12, {1, 1, 1}, 2);
    for (V axis : {V{1, 0, 0}, V{-1, 0, 0}, V{0, 1, 0}, V{0, -1, 0}, V{0, 0, 1}, V{0, 0, -1}}) {
      V d = spin(axis);
      cylinder(e.p + d * (e.r * .37f), e.p + d * e.r, e.r * .17f, .015f, {.94f, .97f, 1}, 5);
    }
    for (int plane = 0; plane < 3; plane++)
      for (int i = 0; i < 24; i++) {
        float a = 2 * PI * i / 24, b = 2 * PI * (i + 1) / 24;
        auto ringPoint = [&](float t) {
          V q = plane == 0   ? V{std::cos(t), std::sin(t), 0}
                : plane == 1 ? V{std::cos(t), 0, std::sin(t)}
                             : V{0, std::cos(t), std::sin(t)};
          return e.p + spin(q) * (e.r * .77f);
        };
        line(ringPoint(a), ringPoint(b), .012f, {.87f, .95f, 1});
      }
    rock(e.p + eyeDir * e.r * .37f, e.r * .21f, 0, {1, .12f, .035f}, 0, 4, 8, {1, 1, 1}, 0);
  }
  void invader(const Entity &e) {
    float yaw = std::atan2(-e.v.x, -e.v.z);
    auto at = [&](V p) { return e.p + rotateXYZ(p, 0, yaw, 0); };
    armoredHull(e.p, 1.75f, 1.65f, .30f, yaw, {.46f, .59f, .70f});
    for (int side : {-1, 1}) {
      armoredHull(at({side * .78f, -.04f, .20f}), .85f, 1.25f, .15f, yaw + side * .22f,
                  {.75f, .83f, .91f});
      cylinder(at({side * .62f, .01f, .52f}), at({side * .62f, .01f, .87f}), .16f, .12f,
               {.14f, .22f, .29f}, 10);
      cylinder(at({side * .62f, .01f, .86f}), at({side * .62f, .01f, .93f}), .11f, .13f,
               {1, .30f, .06f}, 10);
      line(at({side * .70f, .18f, -.27f}), at({side * 1.08f, .03f, .42f}), .025f, {1, .19f, .075f});
    }
    int old = material;
    material = 6;
    armoredHull(at({0, .30f, -.08f}), .53f, .84f, .10f, yaw, {.35f, .80f, 1});
    material = old;
    armoredHull(at({0, -.18f, -.18f}), .40f, .65f, .10f, yaw, {.17f, .23f, .30f});
    line(at({-.18f, -.08f, -.52f}), at({.18f, -.08f, -.52f}), .04f, {1, .3f, .1f});
  }
  void enemy(const Entity &e) {
    switch (e.kind) {
    case Rock:
    case Shard: {
      constexpr Color colors[] = {
          {.98f, .76f, .50f}, {.74f, .85f, .94f}, {.94f, .64f, .43f}, {.88f, .86f, .72f}};
      asteroid(e.p, e.r, e.angle, colors[e.seed % 4], e.seed);
      break;
    }
    case Spinner:
    case SmallSpinner:
      sphericalBomb(e);
      break;
    case Missile:
      // Guided threats are blinking stars, drawn in the emissive pass.
      break;
    case Ufo:
      invader(e);
      break;
    case Salvo:
      // Draw ship shots in the emissive pass so lighting cannot darken them.
      break;
    }
  }
  void cylinder(V a, V b, float ra, float rb, Color c, int segs = 24) {
    V savedOrigin = uvOrigin;
    uvOrigin = a;
    V axis = norm(b - a),
      right = norm(cross(axis, std::abs(axis.y) > .95f ? V{1, 0, 0} : V{0, 1, 0})),
      up = cross(axis, right);
    for (int i = 0; i < segs; i++) {
      float t0 = 2 * PI * i / segs, t1 = 2 * PI * (i + 1) / segs;
      V d0 = right * std::cos(t0) + up * std::sin(t0),
        d1 = right * std::cos(t1) + up * std::sin(t1);
      tri(a + d0 * ra, a + d1 * ra, b + d1 * rb, c);
      tri(a + d0 * ra, b + d1 * rb, b + d0 * rb, c);
      tri(b, b + d0 * rb, b + d1 * rb, c);
      tri(a, a + d1 * ra, a + d0 * ra, c);
    }
    uvOrigin = savedOrigin;
  }
  void billboardRing(V p, float radius, Color c) {
    for (int i = 0; i < 24; i++) {
      float a = i * 2 * PI / 24, b = (i + 1) * 2 * PI / 24;
      line(p + (viewRight * std::cos(a) + viewUp * std::sin(a)) * radius,
           p + (viewRight * std::cos(b) + viewUp * std::sin(b)) * radius, .018f, c);
    }
  }
  void groundRing(V p, float radius, float thick, Color c, int segments = 40) {
    auto point = [&](float angle, float r) {
      float x = p.x + std::cos(angle) * r, z = p.z + std::sin(angle) * r;
      return V{x, terrainHeight(x, z) + p.y, z};
    };
    for (int i = 0; i < segments; i++) {
      float a = 2 * PI * i / segments, b = 2 * PI * (i + 1) / segments;
      tri(point(a, radius), point(b, radius), point(b, radius + thick), c, false);
      tri(point(a, radius), point(b, radius + thick), point(a, radius + thick), c, false);
    }
  }
  void groundDisc(V p, float radius, Color c) {
    V center{p.x, terrainHeight(p.x, p.z) + p.y, p.z};
    for (int i = 0; i < 32; i++) {
      float a = 2 * PI * i / 32, b = 2 * PI * (i + 1) / 32;
      Color edge = c;
      edge.a = 0;
      c.a = .38f;
      auto point = [&](float t) {
        float x = p.x + std::cos(t) * radius, z = p.z + std::sin(t) * radius;
        return V{x, terrainHeight(x, z) + p.y, z};
      };
      vertex(center, c);
      vertex(point(b), edge);
      vertex(point(a), edge);
    }
  }
  void prepareSandSector(int sector) {
    auto &mesh = sectorSandMeshes[sector - 1];
    if (!mesh.empty())
      return;
    constexpr int rings = 32, segments = 96;
    Terrain ground = Terrain::sector(sector);
    std::vector<V> points((rings + 1) * segments), normals(points.size());
    for (int ring = 0; ring <= rings; ++ring)
      for (int segment = 0; segment < segments; ++segment) {
        float radius = 9.2f * ring / rings, angle = 2 * PI * segment / segments;
        float x = radius * std::cos(angle), z = radius * std::sin(angle);
        int i = ring * segments + segment;
        points[i] = {x, ground.height(x, z), z};
        // Blend height gradients rather than normalized normals, so slopes
        // match the blended collision surface throughout the morph.
        normals[i] = ground.gradientNormal(x, z);
      }
    mesh.reserve(rings * segments * 6 + segments * 6);
    auto put = [&](V p, V n, Color c) {
      mesh.push_back({p.x, p.y, p.z, c.r, c.g, c.b, c.a, p.x, p.z, n.x, n.y, n.z, 10});
    };
    auto index = [](int ring, int segment) { return ring * segments + segment % segments; };
    auto surfaceVertex = [&](int i) { put(points[i], normals[i], {1, 1, 1}); };
    for (int ring = 0; ring < rings; ++ring)
      for (int j = 0; j < segments; ++j) {
        int a = index(ring, j), b = index(ring + 1, j), c = index(ring + 1, j + 1),
            d = index(ring, j + 1);
        for (int i : {a, c, b})
          surfaceVertex(i);
        if (ring > 0)
          for (int i : {a, d, c})
            surfaceVertex(i);
      }
    for (int i = 0; i < segments; ++i) {
      float a = 2 * PI * i / segments, b = 2 * PI * (i + 1) / segments;
      V p = points[index(rings, i)], q = points[index(rings, i + 1)],
        lo{std::cos(a) * 9.35f, -.18f, std::sin(a) * 9.35f},
        hi{std::cos(b) * 9.35f, -.18f, std::sin(b) * 9.35f};
      V n = norm(cross(q - p, lo - p));
      for (V v : {p, q, lo, q, hi, lo})
        put(v, n, {.78f, .78f, .78f});
    }
  }
  void sandSurface() {
    if (sandMesh.empty() || sandWeights != terrain.weights) {
      bool first = true;
      for (int sector = 0; sector < 6; ++sector) {
        float weight = terrain.weights[sector];
        if (weight <= 0)
          continue;
        prepareSandSector(sector + 1);
        const auto &mesh = sectorSandMeshes[sector];
        if (first) {
          sandMesh = mesh;
          for (auto &v : sandMesh) {
            v.y *= weight;
            v.nx *= weight;
            v.ny *= weight;
            v.nz *= weight;
          }
          first = false;
        } else {
          for (size_t i = 0; i < mesh.size(); ++i) {
            sandMesh[i].y += mesh[i].y * weight;
            sandMesh[i].nx += mesh[i].nx * weight;
            sandMesh[i].ny += mesh[i].ny * weight;
            sandMesh[i].nz += mesh[i].nz * weight;
          }
        }
      }
      sandWeights = terrain.weights;
    }
    verts.insert(verts.end(), sandMesh.begin(), sandMesh.end());
  }
  void rover(const Game &g) {
    float yaw = std::atan2(-g.aimDir.x, -g.aimDir.z);
    if (g.aimDir.x * g.aimDir.x + g.aimDir.z * g.aimDir.z < .04f)
      yaw = g.cameraAngle;
    int hullMaterial = material;
    material = 9;
    V up = terrainNormal(g.x, g.z), facing{std::sin(yaw), 0, std::cos(yaw)};
    facing = norm(facing - up * dot(facing, up));
    V bodyRight = norm(cross(up, facing));
    V p{g.x, g.groundHeight() + .36f, g.z};
    auto at = [&](V local) { return p + bodyRight * local.x + up * local.y + facing * local.z; };
    armoredHull(p, 1.16f, 1.48f, .24f, yaw, {.94f, .92f, .82f}, up);
    for (int side : {-1, 1}) {
      armoredHull(at({side * .49f, -.06f, .16f}), .42f, 1.08f, .19f, yaw, {.075f, .095f, .12f}, up);
      cylinder(at({side * .49f, .0f, .48f}), at({side * .49f, .0f, .69f}), .13f, .09f,
               {.14f, .75f, 1}, 8);
      line(at({side * .43f, .20f, -.32f}), at({side * .49f, .17f, .29f}), .014f,
           {.22f, .49f, .60f});
    }
    for (int side : {-1, 1}) {
      // Recessed service panels, fasteners and short amber identification marks.
      armoredHull(at({side * .36f, .245f, -.21f}), .10f, .29f, .013f, yaw, {.76f, .39f, .095f}, up);
      for (int vent = 0; vent < 3; vent++)
        armoredHull(at({side * .47f, .135f, .02f + vent * .12f}), .15f, .045f, .008f, yaw,
                    {.035f, .045f, .055f}, up);
      for (float z : {-.31f, .32f})
        cylinder(at({side * .34f, .245f, z}), at({side * .34f, .267f, z}), .025f, .022f,
                 {.38f, .43f, .48f}, 6);
    }
    int old = material;
    material = 6;
    armoredHull(at({0, .24f, -.30f}), .40f, .54f, .10f, yaw, {.3f, .8f, 1}, up);
    material = old;
    armoredHull(at({0, .28f, .15f}), .57f, .54f, .18f, yaw, {.66f, .73f, .79f}, up);
    V mount = g.cannonMuzzle() - V{0, .10f, 0};
    V right = norm(cross(g.aimDir, {0, 1, 0}));
    if (dot(right, right) < .1f)
      right = {1, 0, 0};
    for (int side : {-1, 1}) {
      V root = mount + right * (side * .13f);
      cylinder(root, root + g.aimDir * .67f, .075f, .055f, {.40f, .46f, .51f}, 8);
      cylinder(root + g.aimDir * .58f, root + g.aimDir * .74f, .06f, .045f, {.35f, .95f, 1}, 8);
    }
    groundRing({g.x, .065f, g.z}, .84f, .015f, {.65f, .69f, .65f});
    if (g.spawnFlash > 0)
      billboardRing(p, .9f, {.4f, .85f, 1});
    material = hullMaterial;
  }
  void world(const Game &g, bool title = false) {
    terrain = g.terrain;
    // Prepare the destination during the music fade-out, before the silent morph.
    prepareSandSector(g.level);
    Color ac = accents[g.level - 1];
    camera(g.cameraAngle, cameraShake(g));
    if (!g.blasts.empty()) {
      const auto &b = g.blasts.back();
      impactPosition = b.p;
      impactPower = (1 - b.age / b.duration) * b.radius;
    }
    if (g.dying()) {
      impactPosition = g.deathPos;
      impactPower = g.deathTimer * 2;
    }
    // Sand dunes replace the traversable metal deck; the support rim stays in place.
    material = 10;
    cylinder({0, -1.15f, 0}, {0, -.18f, 0}, 9.6f, 9.35f, {.72f, .72f, .72f}, 64);
    sandSurface();
    material = 2;
    ring({0, -.55f, 0}, 9.52f, .09f, ac, PI / 2, 64);
    // Eight power pylons and buttresses anchor the platform in three dimensions.
    for (int i = 0; i < 8; i++) {
      float a = i * PI / 4;
      V p{std::cos(a) * 9.2f, 0, std::sin(a) * 9.2f};
      cylinder(p + V{0, -.6f, 0}, p + V{0, 1.0f, 0}, .45f, .27f, {.25f, .33f, .43f}, 6);
      cylinder(p + V{0, 1.0f, 0}, p + V{0, 1.35f, 0}, .27f, .17f, ac, 6);
      V far = p * 1.24f + V{0, -1.8f, 0};
      cylinder(far, p + V{0, -.4f, 0}, .4f, .22f, {.09f, .13f, .20f}, 6);
    }
    // Starfield surrounds the arena; distant orbital rings supply parallax.
    for (int i = 0; i < 230; i++) {
      float a = i * 2.39996f, y = std::fmod(i * 3.173f, 36.f) - 5;
      float rad = 35 + (i % 7) * 2;
      float size = i % 9 == 0 ? .055f : .025f;
      V p{std::cos(a) * rad, y, std::sin(a) * rad};
      rock(p, size, 0, {.52f, .67f, .86f}, i, 2, 4, {1, 1, 1}, 0);
    }
    rock({-18, 10, -25}, 5.0f, .6f, {ac.r * .25f, ac.g * .25f, ac.b * .25f}, 71, 12, 24);
    ring({-18, 10, -25}, 6.4f, .05f, {ac.r * .5f, ac.g * .5f, ac.b * .5f}, 1.1f, 80);
    for (int i = 0; i < 26; i++) {
      float a = i * 2.4f;
      rock({std::cos(a) * (14 + i % 4), -2.5f - i % 3, std::sin(a) * (14 + i % 4)},
           .8f + (i % 3) * .6f, a, {.07f, .11f, .17f}, i, 4, 7);
    }
    flush();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    // Soft contact shadows and landing indicators.
    for (const auto &e : g.enemies) {
      Color warning = e.kind == Spinner || e.kind == SmallSpinner
                          ? Color{1, .31f, .16f}
                          : Color{ac.r * .50f, ac.g * .50f, ac.b * .50f};
      if (e.p.x * e.p.x + e.p.z * e.p.z < 85) {
        groundDisc({e.p.x, .026f, e.p.z}, e.r * 1.5f, {.005f, .012f, .02f, .38f});
        warning.a = .48f;
        groundRing({e.p.x, .055f, e.p.z}, e.r + .13f, .025f, warning, 24);
      }
      if (e.p.y - terrainHeight(e.p.x, e.p.z) < 4 && (e.kind == Spinner || e.kind == SmallSpinner))
        groundRing({e.p.x, .065f, e.p.z}, .7f + std::sin(g.t * 10) * .13f, .045f, warning, 24);
    }
    if (!g.dying() && !g.over)
      groundDisc({g.x, .028f, g.z}, 1.0f, {0, .01f, .02f, .4f});
    flush();
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    if (!g.dying() && !g.over)
      rover(g);
    auto glowFor = [&](const Entity &e) {
      return !title && !g.over && !g.dying() && g.locked && e.id == g.targetId
                 ? (useTrackedView ? .45f : 1.05f) +
                       (useTrackedView ? .05f : .10f) * std::sin(g.t * 2 * PI * 1.4f)
                 : 0.f;
    };
    for (const auto &e : g.enemies) {
      setTargetGlow(glowFor(e));
      enemy(e);
      if (targetGlow > 0) {
        // setTargetGlow flushed preceding objects, leaving exactly this mesh.
        targetMesh = verts;
        haloStrength = targetGlow;
      }
    }
    setTargetGlow(0);
    for (const auto &b : g.bullets) {
      line(b.p, b.p - norm(b.v) * .8f, .047f, {.25f, 1, 1});
      line(b.p, b.p - norm(b.v) * .55f, .017f, {1, 1, 1});
    }
    if (!title && !g.over && !g.dying()) {
      V p = g.cannonMuzzle() + V{0, .05f, 0};
      for (int i = 1; i < 10; i++)
        line(p + g.aimDir * (i * .4f), p + g.aimDir * (i * .4f + .12f), .013f, {.20f, .65f, .72f});
    }
    flush();
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    for (const auto &e : g.enemies) {
      setTargetGlow(glowFor(e));
      if (e.kind == Spinner || e.kind == SmallSpinner) {
        // Preserve the rotating core, spikes and rings beneath a stronger
        // white halo and a longer, smoothly tapered wake opposite their travel.
        sprite(e.p, e.r * 2.4f, {1.f, 1.f, 1.f, .60f});
        V direction = norm(e.v);
        for (int i = 1; i <= 10; ++i) {
          float fade = 1.f - i / 11.f;
          sprite(e.p - direction * (e.r * .65f + i * .18f), e.r * (.25f + fade * .55f),
                 {1.f, 1.f, 1.f, fade * .55f});
        }
        continue;
      }
      if (e.kind == Missile) {
        // A symmetric, camera-facing eight-point star stays recognizable from
        // every angle. Blink brightly without making the threat disappear.
        bool bright = std::fmod(e.age * 5.f + (e.id % 7) / 7.f, 1.f) < .55f;
        float glow = bright ? 1.f : .30f;
        Color star = bright ? Color{1.f, 1.f, .92f, 1.f} : Color{1.f, .66f, .28f, .55f};
        auto point = [&](int i) {
          float angle = i * PI / 8;
          float radius = e.r * (i % 2 ? .25f : i % 4 ? .85f : 1.5f);
          return e.p + (viewRight * std::cos(angle) + viewUp * std::sin(angle)) * radius;
        };
        sprite(e.p, e.r * 2.f, {1.f, .85f, .48f, glow * .30f});
        size_t starStart = verts.size();
        for (int i = 0; i < 16; ++i)
          tri(e.p, point(i), point(i + 1), star, false);
        if (targetGlow > 0) {
          targetMesh.assign(verts.begin() + starStart, verts.end());
          haloStrength = targetGlow;
        }
        sprite(e.p, e.r * .43f, {1.f, 1.f, 1.f, glow * .8f});
        continue;
      }
      if (e.kind != Salvo)
        continue;
      V direction = norm(e.v);
      // Layered red halos and a hot core remain readable on both sand and sky.
      // Depth testing stays enabled so the glow cannot show through terrain.
      for (int i = 4; i >= 1; --i) {
        float fade = 1.f - i / 5.f;
        sprite(e.p - direction * (i * .16f), .13f + fade * .10f, {1.f, .015f, .005f, fade * .60f});
      }
      sprite(e.p, .48f, {1.f, .005f, .005f, .45f});
      sprite(e.p, .26f, {1.f, .025f, .01f, .90f});
      size_t coreStart = verts.size();
      sprite(e.p, .115f, {1.f, .35f, .24f, 1.f});
      if (targetGlow > 0) {
        targetMesh.assign(verts.begin() + coreStart, verts.end());
        haloStrength = targetGlow;
      }
    }
    setTargetGlow(0);
    for (const auto &b : g.blasts) {
      float f = b.age / b.duration;
      float size = b.radius * (.25f + f * .85f);
      sprite(b.p, size, {b.c.r, b.c.g, b.c.b, (1 - f) * .7f});
      if (f < .70f)
        sprite(b.p, size * .45f, {1, .92f, .68f, (1 - f / .70f) * .95f});
      if (b.p.y - terrainHeight(b.p.x, b.p.z) < 1.2f)
        groundRing({b.p.x, .07f, b.p.z}, .2f + f * b.radius * 2, .045f + f * .04f,
                   {1, .48f, .12f, (1 - f) * .8f}, 64);
      for (int i = 0; i < 5; i++) {
        float a = i * 2.4f;
        V off{std::cos(a) * f * b.radius * .48f, f * .75f, std::sin(a) * f * b.radius * .48f};
        sprite(b.p + off, size * .55f, {1, .28f, .03f, (1 - f) * .28f});
      }
    }
    for (const auto &p : g.particles) {
      float f = p.life / p.maxlife;
      line(p.p, p.p - p.v * .035f, .013f, p.c);
      sprite(p.p, p.size * 2, {p.c.r, p.c.g, p.c.b, f * .65f});
    }
    if (!g.dying() && !g.over) {
      sprite({g.x, g.groundHeight() + .3f, g.z}, .65f, {.15f, .32f, .40f, .10f});
      if (g.shotTimer > (g.classic ? 2.f / 3.f : .32f) - .05f)
        sprite(g.cannonMuzzle() + g.aimDir * .9f, .25f, {.65f, 1, 1, .75f});
    }
    flush();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (const auto &b : g.blasts) {
      float f = b.age / b.duration;
      if (f > .15f) {
        for (int i = 0; i < 3; i++) {
          V p = b.p +
                V{std::sin(i * 2.4f) * f * .6f, f * 1.3f + i * .12f, std::cos(i * 2.4f) * f * .6f};
          sprite(p, b.radius * (.35f + f * .65f), {.15f, .13f, .12f, std::sin(f * PI) * .21f}, -2);
        }
      }
    }
    flush();
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    buildTargetMask();
  }
  void buildTargetMask() {
    if (targetMesh.empty())
      return;
    float left = float(width), right = 0, bottom = float(height), top = 0;
    for (const auto &v : targetMesh) {
      float w = matrix[3] * v.x + matrix[7] * v.y + matrix[11] * v.z + matrix[15];
      if (w <= .001f)
        continue;
      float x =
          ((matrix[0] * v.x + matrix[4] * v.y + matrix[8] * v.z + matrix[12]) / w * .5f + .5f) *
          width;
      float y =
          ((matrix[1] * v.x + matrix[5] * v.y + matrix[9] * v.z + matrix[13]) / w * .5f + .5f) *
          height;
      left = std::min(left, x);
      right = std::max(right, x);
      bottom = std::min(bottom, y);
      top = std::max(top, y);
    }
    float padding = useTrackedView ? 5.f * std::max(1.f, height / 1536.f) : 14.f * height / 480.f;
    haloX = int(clamp(std::floor(left - padding), 0, float(width)));
    haloY = int(clamp(std::floor(bottom - padding), 0, float(height)));
    haloWidth = int(clamp(std::ceil(right + padding), 0, float(width))) - haloX;
    haloHeight = int(clamp(std::ceil(top + padding), 0, float(height))) - haloY;
    if (haloWidth <= 0 || haloHeight <= 0)
      return;
    glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT); // Keep the scene's shared depth for occlusion.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    // Avoid precision differences between the material and mask vertex programs.
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.f, -1.f);
    glUseProgram(maskProgram);
    glUniformMatrix4fv(glGetUniformLocation(maskProgram, "m"), 1, GL_FALSE, matrix);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, targetMesh.size() * sizeof(Vertex), targetMesh.data(),
                 GL_STREAM_DRAW);
    attributes();
    glDrawArrays(GL_TRIANGLES, 0, targetMesh.size());
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    haloReady = true;
  }
  void sprite(V p, float size, Color c, float kind = -1) {
    V a = p - viewRight * size - viewUp * size, b = p + viewRight * size - viewUp * size,
      d = p - viewRight * size + viewUp * size, e = p + viewRight * size + viewUp * size;
    vertex(a, c, 0, 0, {}, kind);
    vertex(b, c, 1, 0, {}, kind);
    vertex(e, c, 1, 1, {}, kind);
    vertex(a, c, 0, 0, {}, kind);
    vertex(e, c, 1, 1, {}, kind);
    vertex(d, c, 0, 1, {}, kind);
  }
  void capture(const char *name) {
    // RGBA/UNSIGNED_BYTE is the portable GLES readback format, including Mesa EGL.
    std::vector<unsigned char> p(size_t(width) * height * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, p.data());
    FILE *f = std::fopen(name, "wb");
    if (f) {
      std::fprintf(f, "P6\n%d %d\n255\n", width, height);
      for (int y = height - 1; y >= 0; y--)
        for (int x = 0; x < width; x++)
          std::fwrite(p.data() + (size_t(y) * width + x) * 4, 1, 3, f);
      std::fclose(f);
    }
  }
};
} // namespace db
