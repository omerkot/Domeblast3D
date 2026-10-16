#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
namespace db {
constexpr float PI = 3.14159265359f;
inline float clamp(float a, float l, float h) { return std::max(l, std::min(h, a)); }
struct V {
  float x = 0, y = 0, z = 0;
  V operator+(V b) const { return {x + b.x, y + b.y, z + b.z}; }
  V operator-(V b) const { return {x - b.x, y - b.y, z - b.z}; }
  V operator*(float b) const { return {x * b, y * b, z * b}; }
};
inline float dot(V a, V b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V cross(V a, V b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline V norm(V a) { return a * (1.f / std::max(.0001f, std::sqrt(dot(a, a)))); }
// A weighted blend of distinct sector layouts, shared by physics and graphics.
struct Terrain {
  std::array<float, 6> weights{1, 0, 0, 0, 0, 0};
  static Terrain sector(int level) {
    Terrain result;
    result.weights.fill(0);
    result.weights[int(clamp(level, 1, 6)) - 1] = 1;
    return result;
  }
  static float sectorHeight(int level, float x, float z) {
    float radius = std::sqrt(x * x + z * z);
    if (level <= 1 || radius >= 9.2f)
      return 0;
    struct Hill {
      float x, z, sx, sz, height;
    };
    static constexpr Hill layouts[5][4] = {
        {{-4, -1.5, 10, 8, 1},
         {2.8, -3.6, 9, 7, 1.25},
         {3.7, 3.5, 8, 10, .85},
         {-2, 4.6, 7, 6, .65}},
        {{-1.3, -4.5, 8, 7, 1},
         {4, 0, 7, 11, 1.25},
         {-3.8, 2.9, 10, 7, .85},
         {2.2, 4.7, 8, 6, .65}},
        {{-4.3, -3.1, 9, 7, 1},
         {0, 2.2, 10, 9, 1.25},
         {4.7, -1.9, 7, 9, .85},
         {-4.5, 3.8, 7, 6, .65}},
        {{0, -2.8, 7, 10, 1}, {3.2, 4, 9, 7, 1.25}, {-4.7, .1, 7, 10, .85}, {4.8, -3.6, 7, 6, .65}},
        {{-3, 4, 10, 7, 1},
         {3.6, -2.4, 9, 8, 1.25},
         {-3.7, -3.8, 8, 7, .85},
         {4.2, 4.7, 7, 6, .65}}};
    // Normalize the different layouts to the previous terrain's 1.3605-unit peak.
    // Sector relief is 0%, 40%, 80%, 120%, 160%, 200% of that reference.
    static constexpr float normalization[] = {1.0403828f, 1.0305966f, .9929691f, 1.0397073f,
                                              1.0691f};
    int index = int(clamp(level, 2, 6)) - 2;
    float h = .06f;
    for (const auto &hill : layouts[index]) {
      float dx = x - hill.x, dz = z - hill.z;
      h += hill.height * std::exp(-(dx * dx / hill.sx + dz * dz / hill.sz));
    }
    h += .06f * std::sin(x * .75f + z * .22f) * std::sin(z * .68f - x * .16f);
    float edge = clamp((9.2f - radius) / 1.8f, 0, 1);
    edge = edge * edge * (3 - 2 * edge);
    return std::max(0.f, h) * edge * normalization[index] * ((index + 1) * .4f);
  }
  float height(float x, float z) const {
    float h = 0;
    for (int i = 1; i < 6; ++i)
      if (weights[i] > 0)
        h += weights[i] * sectorHeight(i + 1, x, z);
    return h;
  }
  V gradientNormal(float x, float z) const {
    constexpr float e = .06f;
    return {(height(x - e, z) - height(x + e, z)) / (2 * e), 1,
            (height(x, z - e) - height(x, z + e)) / (2 * e)};
  }
  V normal(float x, float z) const { return norm(gradientNormal(x, z)); }
  bool occludes(V eye, V target) const {
    auto under = [&](V p) {
      return p.x * p.x + p.z * p.z < 9.2f * 9.2f && p.y < height(p.x, p.z) - .025f;
    };
    if (under(target))
      return true;
    if (std::min(eye.y, target.y) > 4.f)
      return false;
    for (int i = 1; i < 16; i++)
      if (under(eye + (target - eye) * (i / 16.f)))
        return true;
    return false;
  }
  // One atomic snapshot for the audio thread's visibility checks (10 bits per weight).
  uint64_t packed() const {
    uint64_t value = 0;
    for (int i = 0; i < 6; ++i)
      value |= uint64_t(std::lround(clamp(weights[i], 0, 1) * 1023)) << (i * 10);
    return value;
  }
  static Terrain unpack(uint64_t value) {
    Terrain result;
    float total = 0;
    for (int i = 0; i < 6; ++i) {
      result.weights[i] = float((value >> (i * 10)) & 1023);
      total += result.weights[i];
    }
    if (total == 0)
      return Terrain{};
    for (float &weight : result.weights)
      weight /= total;
    return result;
  }
};
struct Color {
  float r, g, b, a = 1;
};
constexpr int thresholds[] = {0, 1000, 5000, 20000, 50000, 100000};
constexpr const char *names[] = {"FIRST LIGHT",       "ION BLUE",    "VIOLET STORM",
                                 "TURQUOISE HORIZON", "SILVER VEIL", "MIDNIGHT INFINITY"};
constexpr Color skies[] = {{.008, .013, .027}, {.025, .075, .22}, {.12, .026, .19},
                           {.025, .19, .20},   {.17, .19, .22},   {.003, .006, .014}};
constexpr Color accents[] = {{.35, .90, 1}, {.36, .61, 1}, {.85, .43, 1},
                             {.25, 1, .78}, {.9, .94, 1},  {1, .64, .29}};
inline int levelFor(int score) {
  int l = 1;
  for (int i = 1; i < 6; i++)
    if (score >= thresholds[i])
      l = i + 1;
  return l;
}
enum Kind { Rock, Shard, Spinner, SmallSpinner, Missile, Ufo, Salvo };
inline int value(Kind k) {
  constexpr int v[] = {5, 10, 20, 40, 25, 50, 0};
  return v[k];
}
struct Entity {
  Kind kind;
  V p, v;
  float r, angle = 0, age = 0, timer = 1;
  uint32_t seed = 1, id = 0;
};
struct Bullet {
  V p, old, v;
  float life = 2;
};
struct Particle {
  V p, v;
  Color c;
  float life, maxlife, size;
};
struct Blast {
  V p;
  Color c;
  float age = 0, duration = 1, radius = 1;
};
struct SoundEvent {
  int type = 0;
  V p{}, velocity{};
  float radius = 0;
  bool spatial = false;
  SoundEvent(int value = 0) : type(value) {}
  SoundEvent(int value, V position, float extent, V movement = {})
      : type(value), p(position), velocity(movement), radius(extent), spatial(true) {}
};
struct Input {
  float move = 0, movey = 0, aimx = 0, aimy = 0, orbit = 0;
  bool fire = false, focus = false;
};
struct Game {
  uint32_t rng = 0x915ad, nextEntityId = 1, targetId = 0;
  int score = 0, peak = 0, lives = 5, level = 1, kills = 0, shots = 0, high = 0, practice = 0;
  bool over = false, classic = false, autoFire = true, invincible = false;
  float x = 0, z = 4, aim = 0, cameraAngle = .45f, t = 0, shotTimer = 0, spawnTimer = .8,
        deathTimer = 0, spawnFlash = 0, flash = 0, banner = 3, ufoTimer = 17, shake = 0;
  V deathPos{};
  int deathCount = 0, lastUfoEntry = -1;
  bool spinnerDeath = false;
  V aimDir{0, 1, 0}, lockPoint{}, targetPoint{};
  float targetRadius = .6f;
  bool locked = false, menuSceneActive = false;
  std::vector<Entity> enemies;
  std::vector<Bullet> bullets;
  std::vector<Particle> particles;
  std::vector<Blast> blasts;
  std::vector<SoundEvent> sounds;
  Terrain terrain, terrainStart;
  int terrainTarget = 1;
  float terrainProgress = 1, terrainElapsed = 0;
  float terrainHeight(float px, float pz) const { return terrain.height(px, pz); }
  V terrainNormal(float px, float pz) const { return terrain.normal(px, pz); }
  bool terrainOccludes(V eye, V target) const { return terrain.occludes(eye, target); }
  float groundHeight() const { return terrainHeight(x, z); }
  void setTerrainSector(int sector) {
    terrain = terrainStart = Terrain::sector(sector);
    terrainTarget = sector;
    terrainProgress = 1;
    terrainElapsed = 0;
  }
  void beginTerrainTransition() {
    terrainStart = terrain;
    terrainTarget = level;
    terrainProgress = terrainElapsed = 0;
  }
  void syncTerrain(uint32_t musicTimeline, float dt, bool audioAvailable) {
    if (terrainProgress >= 1 || menuSceneActive)
      return;
    terrainElapsed += dt;
    float progress = terrainProgress;
    // The mixer publishes sector + sample-accurate progress through its silent gap.
    if (audioAvailable) {
      if (int(musicTimeline >> 16) == terrainTarget)
        progress = std::max(progress, (musicTimeline & 65535) / 44100.f);
    } else {
      progress = clamp(terrainElapsed - 1.f, 0, 1);
    }
    terrainProgress = clamp(progress, 0, 1);
    float ease = terrainProgress * terrainProgress * (3 - 2 * terrainProgress);
    for (int i = 0; i < 6; ++i)
      terrain.weights[i] =
          terrainStart.weights[i] * (1 - ease) + (i == terrainTarget - 1 ? ease : 0);
    // Ground-following stars ride sinking hills as well as rising ones.
    for (auto &e : enemies)
      if (e.kind == Missile && e.v.y == 0)
        e.p.y = terrain.height(e.p.x, e.p.z) + .65f;
  }
  V cannonCenter() const { return {x, groundHeight() + .5f, z}; }
  V cannonMuzzle() const { return {x, groundHeight() + 1.05f, z}; }
  bool hasSpaceship() const {
    return std::any_of(enemies.begin(), enemies.end(),
                       [](const Entity &e) { return e.kind == Ufo; });
  }
  bool spaceshipDucking() const {
    return !menuSceneActive &&
           (hasSpaceship() || (!over && !dying() && level >= 4 && ufoTimer <= 1.f));
  }
  float random() {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (rng & 0xffffff) / float(0x1000000);
  }
  float range(float a, float b) { return a + (b - a) * random(); }
  void reset(int sector = 0) {
    int h = high;
    bool c = classic, af = autoFire;
    *this = Game();
    high = h;
    classic = c;
    autoFire = af;
    practice = sector;
    score = peak = sector ? thresholds[sector - 1] : 0;
    level = levelFor(peak);
    setTerrainSector(level);
    rng += sector * 177;
  }
  void addScore(int n) {
    score = std::max(0, score + n);
    if (score > peak) {
      int old = peak;
      peak = score;
      lives += (peak / 1000 - old / 1000);
      if (peak / 1000 > old / 1000)
        sounds.push_back(5);
      if (!practice)
        high = std::max(high, peak);
    }
    int next = levelFor(peak);
    if (next != level) {
      level = next;
      beginTerrainTransition();
      banner = 4;
      sounds.push_back(6);
    }
  }
  void burst(V p, Color c, int count = 20, float power = 1) {
    for (int i = 0; i < count && particles.size() < 700; i++) {
      float life = range(.25, .9);
      particles.push_back({p,
                           {range(-4, 4) * power, range(-3, 4) * power, range(-3, 3) * power},
                           c,
                           life,
                           life,
                           range(.025, .09)});
    }
  }
  void updateTitle(float time, float dt, int sector) {
    // Allocate the attract scene once. Re-randomizing seeds/velocity each frame
    // changed asteroid geometry and flipped UFO headings in the old menu.
    if (!menuSceneActive) {
      enemies.clear();
      bullets.clear();
      particles.clear();
      blasts.clear();
      deathTimer = flash = 0;
      locked = false;
      over = false;
      for (int i = 0; i < 8; i++) {
        Kind kind = i == 1 ? Spinner : i == 5 ? Ufo : Rock;
        Entity e{};
        e.kind = kind;
        e.r = kind == Ufo ? 1.1f : kind == Spinner ? .4f : .4675f;
        e.seed = 0x571a2u + uint32_t(i) * 7919u;
        e.id = 10000 + i;
        e.v = kind == Ufo ? V{1, 0, .1f} : V{0, -.3f, 0};
        enemies.push_back(e);
      }
      menuSceneActive = true;
    }
    t += dt;
    level = sector ? sector : 1;
    setTerrainSector(level);
    x = std::sin(time * .4f) * 3;
    z = std::cos(time * .4f) * 3;
    cameraAngle += dt * .09f;
    aimDir = norm(V{std::sin(time * .6f) * .2f, 1, .1f});
    for (size_t i = 0; i < enemies.size(); i++) {
      float phase = std::fmod(i * 2.7f + 10 - time * .3f, 10.f);
      if (phase < 0)
        phase += 10;
      enemies[i].p = {std::sin(i * 13.1f) * 7, 2 + phase, std::cos(i * 4.3f) * 6};
      enemies[i].angle = time * .5f + i;
    }
  }
  bool dying() const { return deathTimer > 0; }
  void explode(V p, Color c, float radius = 1, float duration = .8f) {
    if (blasts.size() < 40)
      blasts.push_back({p, c, 0, duration, radius});
  }
  void lose(bool spinner = false) {
    if (over || dying() || invincible)
      return;
    lives--;
    addScore(-100 * level);
    deathCount++;
    spinnerDeath = spinner;
    deathTimer = 1.5f;
    deathPos = {x, groundHeight() + .65f, z};
    locked = false;
    bullets.clear();
    flash = 1.5f;
    shake = .7f;
    burst(deathPos, {1, .55f, .14f}, 110, 2.3f);
    for (auto &p : particles) {
      p.life = std::max(p.life, 1.25f);
      p.maxlife = std::max(p.maxlife, 1.25f);
    }
    explode(deathPos, {1, .35f, .06f}, 3.1f, 1.5f);
    sounds.emplace_back(3, deathPos, .9f);
  }
  Entity make(Kind k, float px, float py) {
    float r = k == Rock           ? .55
              : k == Shard        ? .28
              : k == Spinner      ? .4
              : k == SmallSpinner ? .23
              : k == Ufo          ? 1.1
                                  : .2;
    return {k,
            {px, py, 0},
            {range(-.24, .24), -(.9f + .23f * level), 0},
            r,
            range(0, 6),
            0,
            range(.8, 1.7) / (k == Ufo ? 1.5f : 1.f),
            rng,
            nextEntityId++};
  }
  void spawn() {
    if (hasSpaceship())
      return;
    float p = random();
    Kind k = Rock;
    if (p < .12)
      k = SmallSpinner;
    else if (p < .25)
      k = Spinner;
    else if (p < .34 && level >= 2)
      k = Missile;
    float theta = range(0, 2 * PI), radius = std::sqrt(random()) * 8.3f;
    Entity e = make(k, std::cos(theta) * radius, range(12, 16));
    e.p.z = std::sin(theta) * radius;
    e.v.z = range(-.14f, .14f);
    if (k == SmallSpinner)
      e.v.y *= 1.22;
    if (k == Missile)
      e.v.y *= 1.35;
    enemies.push_back(e);
  }
  void kill(size_t i) {
    Entity e = enemies[i];
    enemies.erase(enemies.begin() + i);
    addScore(value(e.kind) * level);
    kills++;
    Color c = e.kind == Rock || e.kind == Shard ? Color{1, .63, .25} : Color{.5, 1, 1};
    burst(e.p, c, e.kind == Ufo ? 40 : 18);
    explode(e.p, c, e.kind == Ufo ? 1.7f : e.kind == Rock ? 1.1f : .65f);
    int effect = e.kind == Ufo                                 ? 9
                 : e.kind == Spinner || e.kind == SmallSpinner ? 8
                 : e.kind == Missile                           ? 10
                                                               : 2;
    sounds.emplace_back(effect, e.p, e.r);
    shake = std::max(shake, .07f);
    if (e.kind == Rock && !hasSpaceship() && random() < .64) {
      for (int s : {-1, 1}) {
        Entity n = make(Shard, e.p.x + s * .18f, e.p.y);
        n.p.z = e.p.z;
        n.v = {s * range(.4, 1.1), e.v.y * 1.18f, s * range(-.6, .6)};
        enemies.push_back(n);
      }
    }
  }
  void update(float dt, const Input &in) {
    t += dt;
    banner = std::max(0.f, banner - dt);
    flash = std::max(0.f, flash - dt);
    shake = std::max(0.f, shake - dt);
    for (auto &p : particles) {
      p.life -= dt;
      p.p = p.p + p.v * dt;
      p.v.y -= 2 * dt;
    }
    particles.erase(
        std::remove_if(particles.begin(), particles.end(), [](auto &p) { return p.life <= 0; }),
        particles.end());
    for (auto &b : blasts)
      b.age += dt;
    blasts.erase(std::remove_if(blasts.begin(), blasts.end(),
                                [](const auto &b) { return b.age >= b.duration; }),
                 blasts.end());
    spawnFlash = std::max(0.f, spawnFlash - dt);
    if (over)
      return;
    if (dying()) {
      deathTimer = std::max(0.f, deathTimer - dt);
      if (deathTimer == 0) {
        if (lives <= 0) {
          over = true;
          return;
        }
        x = 0;
        z = 4;
        aimDir = {0, 1, 0};
        locked = false;
        shotTimer = .15f;
        spawnFlash = .45f;
        // A replacement cannon always starts on a completely empty board.
        enemies.clear();
        bullets.clear();
        particles.clear();
        blasts.clear();
        sounds.clear();
        targetId = 0;
        targetPoint = lockPoint = {};
        spawnTimer = .8f;
        ufoTimer = 17;
        flash = shake = 0;
      }
      return;
    }
    cameraAngle += in.orbit * dt * .9f;
    V right{std::cos(cameraAngle), 0, -std::sin(cameraAngle)};
    V back{std::sin(cameraAngle), 0, std::cos(cameraAngle)};
    V motion = right * in.move + back * in.movey;
    if (dot(motion, motion) > 1)
      motion = norm(motion);
    V slope = terrainNormal(x, z);
    float climb = -(slope.x * motion.x + slope.z * motion.z) / slope.y;
    float stride = (in.focus ? 3.5f : 7.f) * dt / std::sqrt(1 + climb * climb);
    x += motion.x * stride;
    z += motion.z * stride;
    float radius = std::sqrt(x * x + z * z);
    if (radius > 8.2f) {
      x *= 8.2f / radius;
      z *= 8.2f / radius;
    }
    float stick = std::sqrt(in.aimx * in.aimx + in.aimy * in.aimy);
    locked = false;
    V muzzle = cannonMuzzle();
    if (stick > .2f || in.fire) {
      // Stick direction is screen-relative. Elevation assist selects a threat
      // in that direction; lead calculation produces a real 3D trajectory.
      V screenUp = norm(V{-back.x * 6.f, 23.f, -back.z * 6.f});
      float best = -1e9f;
      for (const auto &e : enemies) {
        if (terrainOccludes(muzzle, e.p))
          continue;
        V delta = e.p - muzzle;
        float dx = dot(delta, right), dy = dot(delta, screenUp);
        float len = std::sqrt(dx * dx + dy * dy);
        float alignment = stick > .2f
                              ? (dx * in.aimx - dy * in.aimy) / (std::max(.01f, len * stick))
                              : delta.y / std::max(.01f, std::sqrt(dot(delta, delta)));
        if (alignment < .76f)
          continue;
        float priority = alignment * 20 - std::sqrt(dot(delta, delta)) * .22f +
                         (e.kind == Spinner || e.kind == SmallSpinner ? 1.1f : 0);
        if (priority > best) {
          best = priority;
          locked = true;
          targetId = e.id;
          targetPoint = e.p;
          targetRadius = e.r + .18f;
          float speed = peak >= 1000000 ? 1.9f : peak >= 500000 ? 1.6f : peak >= 200000 ? 1.3f : 1;
          V velocity = e.v * speed;
          float qa = dot(velocity, velocity) - 24 * 24, qb = 2 * dot(delta, velocity),
                qc = dot(delta, delta);
          float discriminant = std::max(0.f, qb * qb - 4 * qa * qc);
          float travel = std::abs(qa) > .001f ? (-qb - std::sqrt(discriminant)) / (2 * qa)
                                              : std::sqrt(qc) / 24;
          lockPoint = e.p + velocity * clamp(travel, 0, 1.8f);
        }
      }
      V targetDir = locked ? norm(lockPoint - muzzle)
                           : norm(right * in.aimx + back * in.aimy + V{0, 1.4f, 0});
      aimDir = norm(aimDir + (targetDir - aimDir) * std::min(1.f, dt * 18));
    } else {
      aimDir = norm(aimDir + (V{0, 1, 0} - aimDir) * std::min(1.f, dt * 5));
    }
    aim = std::atan2(aimDir.x, aimDir.z);
    shotTimer -= dt;
    if ((in.fire || autoFire) && shotTimer <= 0) {
      V v = aimDir * 24;
      V p = cannonMuzzle() + aimDir * .65f;
      bullets.push_back({p, p, v});
      shotTimer = classic ? 2.f / 3.f : .32f;
      shots++;
      sounds.emplace_back(1, cannonMuzzle(), .7f);
    }
    float extra = peak >= 1000000 ? 1.9f : peak >= 500000 ? 1.6f : peak >= 200000 ? 1.3f : 1;
    bool shipPresent = hasSpaceship();
    if (level >= 4 && !shipPresent) {
      ufoTimer -= dt;
      if (ufoTimer < 0) {
        // Choose an entry point around the full perimeter and cross toward the
        // opposite side, with a varied offset through the arena interior.
        int side = lastUfoEntry < 0 ? int(range(0, 8)) : (lastUfoEntry + 2 + int(range(0, 5))) % 8;
        lastUfoEntry = side;
        float entry = side * PI / 4 + range(-.24f, .24f);
        V start{std::cos(entry) * 11.3f, 8.5f, std::sin(entry) * 11.3f};
        V destination{-start.x + range(-2.5f, 2.5f), 8.5f, -start.z + range(-2.5f, 2.5f)};
        Entity e = make(Ufo, start.x, start.y);
        e.p = start;
        e.v = norm(destination - start) * (1.7f + .15f * level);
        enemies.push_back(e);
        shipPresent = true;
        ufoTimer = range(12, 18);
        sounds.emplace_back(4, e.p, e.r, e.v);
      }
    }
    // A ship encounter suspends the background wave and its timer. Only the
    // existing assets and combat projectiles continue until the ship is gone.
    if (!shipPresent) {
      spawnTimer -= dt;
      if (spawnTimer <= 0) {
        if (enemies.size() < 90)
          spawn();
        spawnTimer = range(.55, .9) / ((.8 + .20 * level) * extra);
      }
    }
    std::vector<Entity> salvos;
    for (size_t i = 0; i < enemies.size();) {
      auto &e = enemies[i];
      e.age += dt;
      e.angle += dt * (e.kind == Spinner || e.kind == SmallSpinner ? 7 : 1.2);
      if (e.kind == Missile) {
        V delta = V{x, 0, z} - V{e.p.x, 0, e.p.z};
        float ground = terrainHeight(e.p.x, e.p.z);
        V heading = norm(delta) * (e.p.y - ground < .8f ? 4.f : 2.f);
        e.v.x += (heading.x - e.v.x) * dt * 1.8f;
        e.v.z += (heading.z - e.v.z) * dt * 1.8f;
        if (e.p.y < ground + .65f) {
          e.p.y = ground + .65f;
          e.v.y = 0;
        }
      }
      if (e.kind == Ufo) {
        e.p.y = 8.5f + std::sin(e.age * 2) * .25f;
        e.timer -= dt;
        if (e.timer < 0) {
          Entity s = make(Salvo, e.p.x, e.p.y - .3);
          s.p.z = e.p.z;
          s.v = norm(cannonCenter() - e.p) * (3.5f + .2f * level);
          salvos.push_back(s);
          e.timer = 1.f;
          sounds.emplace_back(11, e.p, e.r, e.v);
        }
      }
      V previous = e.p;
      e.p = e.p + e.v * (dt * extra);
      if (e.kind == Missile && e.v.y == 0)
        e.p.y = terrainHeight(e.p.x, e.p.z) + .65f;
      V travel = e.p - previous;
      float closest = clamp(
          dot(cannonCenter() - previous, travel) / std::max(.00001f, dot(travel, travel)), 0, 1);
      V near = previous + travel * closest;
      if (dot(near - cannonCenter(), near - cannonCenter()) < (e.r + .45f) * (e.r + .45f)) {
        lose();
        enemies.erase(enemies.begin() + i);
        if (dying())
          return;
        continue;
      }
      float ground = terrainHeight(e.p.x, e.p.z);
      if (e.p.y <= ground + e.r) {
        if (e.kind == Spinner || e.kind == SmallSpinner) {
          lose(true);
          burst({e.p.x, ground + .06f, e.p.z}, {1, .25, .18}, 30);
        } else if (e.kind == Rock || e.kind == Shard) {
          addScore(-(e.kind == Rock ? 5 : 10) * level);
          burst({e.p.x, ground + .06f, e.p.z}, {1, .4, .12}, 14);
          explode({e.p.x, ground + .08f, e.p.z}, {1, .35f, .1f}, .8f);
        }
        enemies.erase(enemies.begin() + i);
        if (dying())
          return;
        continue;
      }
      if (std::abs(e.p.x) > 12 || std::abs(e.p.z) > 12 || e.age > 30) {
        enemies.erase(enemies.begin() + i);
        continue;
      }
      i++;
    }
    for (auto &s : salvos)
      enemies.push_back(s);
    for (size_t b = 0; b < bullets.size();) {
      auto &shot = bullets[b];
      shot.old = shot.p;
      shot.p = shot.p + shot.v * dt;
      shot.life -= dt;
      bool groundHit = false;
      V clearPoint = shot.old;
      for (int sample = 1; sample <= 4; sample++) {
        V candidate = shot.old + (shot.p - shot.old) * (sample * .25f);
        if (candidate.y <= terrainHeight(candidate.x, candidate.z) + .025f) {
          V lo = clearPoint, hi = candidate;
          for (int step = 0; step < 7; step++) {
            V mid = (lo + hi) * .5f;
            if (mid.y > terrainHeight(mid.x, mid.z) + .025f)
              lo = mid;
            else
              hi = mid;
          }
          shot.p = hi;
          groundHit = true;
          break;
        }
        clearPoint = candidate;
      }
      bool hit = false;
      for (size_t i = 0; i < enemies.size(); i++) {
        auto &e = enemies[i];
        V d = shot.p - shot.old;
        float q = clamp(dot(e.p - shot.old, d) / std::max(.0001f, dot(d, d)), 0, 1);
        V near = shot.old + d * q;
        if (dot(e.p - near, e.p - near) < (e.r + .13f) * (e.r + .13f)) {
          kill(i);
          hit = true;
          break;
        }
      }
      if (groundHit && !hit && shot.p.x * shot.p.x + shot.p.z * shot.p.z < 9.2f * 9.2f)
        burst(shot.p, {.72f, .55f, .31f}, 5, .35f);
      if (hit || groundHit || shot.life < 0 || shot.p.y > 20 || std::abs(shot.p.x) > 15 ||
          std::abs(shot.p.z) > 15 || shot.p.y < 0) {
        bullets.erase(bullets.begin() + b);
      } else
        b++;
    }
    if (locked) {
      auto target = std::find_if(enemies.begin(), enemies.end(),
                                 [&](const auto &e) { return e.id == targetId; });
      if (target == enemies.end())
        locked = false;
      else {
        targetPoint = target->p;
        targetRadius = target->r + .18f;
      }
    }
  }
};
} // namespace db
