#pragma once
#include "game.hpp"
namespace db {
constexpr float cameraHeight = 9, cameraDistance = 23, cameraTargetHeight = 3;
constexpr float cameraFov = 41.20442f; // Retains the previous 514px deck width.
inline float viewportAspect = 4.f / 3;
inline bool adaptWebViewport = false;
inline float webLandscapeBlend() {
  return adaptWebViewport ? clamp((viewportAspect - 4.f/3) * 1.5f, 0.f, 1.f) : 0.f;
}
inline float cameraVerticalTangent() {
  float tangent=std::tan(cameraFov * PI / 360);
  // Portrait retains the original horizontal coverage. Wide screens need sky
  // above the arena, not just extra scenery on either side of a narrow view.
  float portrait=adaptWebViewport ? std::max(1.f,(4.f/3)/std::max(.1f,viewportAspect)) : 1.f;
  float landscape=webLandscapeBlend();
  return std::max(tangent * portrait, tangent + (std::tan(70.f * PI / 360) - tangent) * landscape);
}
inline float cameraTargetY() { return cameraTargetHeight + 3.f * webLandscapeBlend(); }
struct CameraView {
  V eye, forward, right, up;
  Terrain terrain;
  bool tracked = false;
  std::array<float, 16> clip{};
  static CameraView fromTracked(const float *projectionView, const float *worldFromEye,
                                const Terrain &ground = {}) {
    CameraView view(0, 0, ground);
    view.tracked = true;
    std::copy(projectionView, projectionView + 16, view.clip.begin());
    view.eye = {worldFromEye[12], worldFromEye[13], worldFromEye[14]};
    view.right = norm({worldFromEye[0], worldFromEye[1], worldFromEye[2]});
    view.up = norm({worldFromEye[4], worldFromEye[5], worldFromEye[6]});
    view.forward = norm({-worldFromEye[8], -worldFromEye[9], -worldFromEye[10]});
    return view;
  }
  explicit CameraView(float angle, float shake = 0, const Terrain &ground = {}) : terrain(ground) {
    eye = {std::sin(angle) * cameraDistance + shake, cameraHeight,
           std::cos(angle) * cameraDistance};
    forward = norm(V{0, cameraTargetY(), 0} - eye);
    right = norm(cross(forward, {0, 1, 0}));
    up = cross(right, forward);
  }
  bool visible(V p, float radius = 0) const {
    if (tracked) {
      // Extract world-space planes from the headset's asymmetric projection.
      for (int axis = 0; axis < 3; ++axis)
        for (int sign : {-1, 1}) {
          V normal{clip[3] + sign * clip[axis], clip[7] + sign * clip[4 + axis],
                   clip[11] + sign * clip[8 + axis]};
          float distance = dot(normal, p) + clip[15] + sign * clip[12 + axis];
          if (distance < -radius * std::sqrt(dot(normal, normal))) return false;
        }
    } else {
    V d = p - eye;
    float z = dot(d, forward);
    if (z + radius < .1f || z - radius > 50.f)
      return false;
    float tanY = cameraVerticalTangent(), tanX = tanY * viewportAspect;
    float vx = dot(d, right), vy = dot(d, up), top = tanY;
    if (std::abs(vx) > z * tanX + radius * std::sqrt(1 + tanX * tanX))
      return false;
    if (vy > z * top + radius * std::sqrt(1 + top * top) ||
        vy < -z * tanY - radius * std::sqrt(1 + tanY * tanY))
      return false;
    }
    // Hills can hide low targets. Test the centre and visible extent so a
    // partially exposed object remains audible and selectable.
    if (terrain.occludes(eye, p) && terrain.occludes(eye, p + up * radius) &&
        terrain.occludes(eye, p + right * radius) && terrain.occludes(eye, p - right * radius))
      return false;
    return true;
  }
};
inline float cameraShake(const Game &g) { return g.shake * std::sin(g.t * 75) * .22f; }
struct ThreatAudio {
  float whistle = 0, missile = 0;
  bool ufo = false;
};
inline ThreatAudio visibleThreatAudio(const Game &g, const CameraView &view) {
  ThreatAudio a;
  float lowest = 2;
  for (const auto &e : g.enemies) {
    if (!view.visible(e.p, e.r))
      continue;
    if (e.kind == Spinner || e.kind == SmallSpinner)
      lowest = std::min(lowest, clamp(e.p.y / 16, .04f, 1));
    if (e.kind == Missile)
      a.missile = std::max(a.missile, clamp(1 - e.p.y / 18, .08f, 1));
    if (e.kind == Ufo)
      a.ufo = true;
  }
  a.whistle = lowest < 2 ? lowest : 0;
  return a;
}
inline ThreatAudio visibleThreatAudio(const Game &g) {
  return visibleThreatAudio(g, CameraView(g.cameraAngle, cameraShake(g), g.terrain));
}
inline V rotateXYZ(V p, float a, float b, float c) {
  p = {p.x, p.y * std::cos(a) - p.z * std::sin(a), p.y * std::sin(a) + p.z * std::cos(a)};
  p = {p.x * std::cos(b) + p.z * std::sin(b), p.y, -p.x * std::sin(b) + p.z * std::cos(b)};
  return {p.x * std::cos(c) - p.y * std::sin(c), p.x * std::sin(c) + p.y * std::cos(c), p.z};
}
inline V asteroidShape(V u, uint32_t seed) {
  int type = seed % 6;
  float phase = (seed % 997) * .031f;
  float relief = .78f + .12f * std::sin(u.x * 5 + phase) * std::cos(u.y * 4 - phase) +
                 .10f * std::sin(u.z * 7 + u.x * 3);
  V s{1, .74f, .82f};
  if (type == 1)
    s = {.48f, 1, .53f}; // Long fractured splinter.
  if (type == 2)
    s = {1, .36f, .83f}; // Flat slab.
  if (type == 3) {       // Angular block with rounded chipped corners.
    auto square = [](float f) { return std::copysign(std::pow(std::abs(f), .48f), f); };
    u = {square(u.x), square(u.y), square(u.z)};
    u = u * .64f;
    s = {1, .9f, 1};
    relief = .96f;
  }
  if (type == 4) {
    s = {1, .69f, .73f};
    relief = .63f + .31f * std::abs(u.x) + .12f * std::sin(u.z * 6 + phase);
  }
  if (type == 5) {
    s = {.78f, .89f, 1};
    relief = .78f + .16f * std::sin(u.x * 8 + u.z * 5 + phase);
    u.x = std::max(u.x, -.34f);
  }
  return {u.x * s.x * relief, u.y * s.y * relief, u.z * s.z * relief};
}
} // namespace db
