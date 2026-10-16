#pragma once
#include "../third_party/stb_vorbis.c"
#include "game.hpp"
#include "geometry.hpp"
#include "music_cache.hpp"
#include <climits>
#ifndef __EMSCRIPTEN__
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <SDL.h>
#include <atomic>
#include <cstdlib>
#include <functional>
namespace db {
// Original composed soundtrack. Native installs map ready-to-play PCM caches;
// uncached installs retain the original Ogg decoder as a fallback. Sector fades
// and the music-only silent gap remain independent of effects.
struct Audio {
#ifdef __EMSCRIPTEN__
  // WebAudio callbacks and game frames share the browser's main thread.
  bool useTrackedView = false;
  CameraView trackedView{0};
#endif
  struct Voice {
    int type = 0;
    float age = 0, dur = 0, phase = 0, noisePhase = 0;
    uint32_t lfsr = 0x1ffff;
    SoundEvent source{};
  };
  SDL_AudioDeviceID device = 0;
  FILE *recording = nullptr;
  std::atomic<int> level{1}, volume{65}, music{1}, active{0}, musicVolume{80}, effectsVolume{100};
  std::atomic<float> whistle{0}, missileThreat{0}, viewAngle{.45f}, viewShake{0};
  std::atomic<bool> sceneVisible{true}, shipDucking{false};
  float encounterGain = 1.f;
  std::atomic<uint64_t> viewTerrain{1023};
  std::atomic<uint32_t> terrainTimeline{(1u << 16) | 44100u};
  std::atomic<int> ufoPresent{0};
  float spinnerPhase = 0, missilePhase = 0, ufoPhase = 0, fxDC = 0;
  struct Track {
    short *pcm = nullptr;
    void *mapping = nullptr;
    size_t mappingBytes = 0;
    int frames = 0;
    int position = 0;
  };
  Track tracks[6];
  static constexpr const char *titles[6] = {"IGNITION BLOOM",   "BLUE SHIFT BOULEVARD",
                                            "VIOLET VELOCITY",  "AURORA AFTERBURN",
                                            "SILVER OVERDRIVE", "MIDNIGHT HOMECOMING"};
  static constexpr const char *artists[6] = {
      "SIX SKIES / ORIGINAL SCORE", "SIX SKIES / ORIGINAL SCORE", "SIX SKIES / ORIGINAL SCORE",
      "SIX SKIES / ORIGINAL SCORE", "SIX SKIES / ORIGINAL SCORE", "SIX SKIES / ORIGINAL SCORE"};
  enum class MusicPhase { Playing, FadeOut, Gap, FadeIn };
  MusicPhase musicPhase = MusicPhase::Playing;
  int currentTrack = 0, pendingTrack = 0, phaseRemaining = 0;
  float transitionGain = 1, fadeOutStart = 1;
  static constexpr float bombGain = .30f;        // Bomb-specific warning/hit effects: 70% quieter.
  static constexpr int transitionFrames = 44100; // One second at the fixed mixer rate.
  bool soundtrackReady = false;
  Voice voices[32];
  SoundEvent queue[128];
  std::atomic<unsigned> write{0}, read{0};
  uint64_t sample = 0;
  uint32_t noiseState = 431;

  static void callback(void *ptr, Uint8 *stream, int len) {
    static_cast<Audio *>(ptr)->render(reinterpret_cast<float *>(stream), len / 8);
  }
  bool mapCachedTrack(int index, const std::string &source) {
#if !defined(__EMSCRIPTEN__) && SDL_BYTEORDER == SDL_LIL_ENDIAN
    std::string cache = "assets/music-cache/sector" + std::to_string(index + 1) + ".pcm";
    int fd = open(cache.c_str(), O_RDONLY);
    if (fd < 0)
      return false;
    struct stat info{};
    MusicCacheHeader header;
    uint64_t sourceBytes = 0, sourceHash = 0;
    bool valid = fstat(fd, &info) == 0 && pread(fd, &header, sizeof(header), 0) == sizeof(header) &&
                 std::memcmp(header.magic, "DBPCM01", 8) == 0 && header.rate == 44100 &&
                 header.channels == 2 && header.frames >= 4410 && header.frames <= INT_MAX &&
                 uint64_t(info.st_size) == sizeof(header) + header.frames * 4 &&
                 musicFingerprint(source.c_str(), sourceBytes, sourceHash) &&
                 header.sourceBytes == sourceBytes && header.sourceHash == sourceHash;
    void *mapping = valid ? mmap(nullptr, info.st_size, PROT_READ, MAP_PRIVATE, fd, 0) : MAP_FAILED;
    close(fd);
    if (mapping == MAP_FAILED)
      return false;
    auto &track = tracks[index];
    track.mapping = mapping;
    track.mappingBytes = info.st_size;
    track.pcm = reinterpret_cast<short *>(static_cast<char *>(mapping) + sizeof(header));
    track.frames = int(header.frames);
    return true;
#else
    (void)index;
    (void)source;
    return false;
#endif
  }
  bool loadTracks(const std::function<void(int, int)> &progress = {}) {
    int loaded = 0;
    for (int i = 0; i < 6; i++) {
      std::string file = "assets/music/sector" + std::to_string(i + 1) + ".ogg";
      bool cached = mapCachedTrack(i, file);
      int channels = cached ? 2 : 0, rate = cached ? 44100 : 0;
      if (!cached)
        tracks[i].frames =
            stb_vorbis_decode_filename(file.c_str(), &channels, &rate, &tracks[i].pcm);
      if (tracks[i].frames > 0 && channels == 2 && rate == 44100) {
        loaded++;
        std::printf("MUSIC %d: %s / %.1f seconds / %s\n", i + 1, titles[i],
                    tracks[i].frames / 44100.f,
                    cached ? "mapped PCM cache" : "decoded Ogg fallback");
      } else {
        std::fprintf(stderr, "MUSIC ERROR: %s (%d ch, %d Hz)\n", file.c_str(), channels, rate);
        std::free(tracks[i].pcm);
        tracks[i].pcm = nullptr;
        tracks[i].frames = 0;
      }
      if (progress)
        progress(i + 1, 6);
    }
    soundtrackReady = loaded == 6;
    return soundtrackReady;
  }
  bool init(const std::function<void(int, int)> &progress = {}) {
    loadTracks(progress);
    SDL_AudioSpec want{}, have{};
    want.freq = 44100;
    want.format = AUDIO_F32SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = callback;
    want.userdata = this;
    device = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (device)
      SDL_PauseAudioDevice(device, 0);
    return device != 0;
  }
  void push(int s) { push(SoundEvent{s}); }
  void push(const SoundEvent &s) {
    unsigned w = write.load(std::memory_order_relaxed);
    if (w - read.load(std::memory_order_acquire) < 128) {
      queue[w % 128] = s;
      write.store(w + 1, std::memory_order_release);
    }
  }
  float noise() {
    noiseState ^= noiseState << 13;
    noiseState ^= noiseState >> 17;
    noiseState ^= noiseState << 5;
    return (noiseState & 65535) / 32768.f - 1;
  }
  // AY-family character: integer pitch divisors, square waves, 17-bit clocked
  // noise and a 16-step logarithmic volume envelope. Original effects, not ROM audio.
  static float psgTone(float &phase, float frequency) {
    float period = clamp(std::round(111860.78f / std::max(frequency, 28.f)), 1, 4095);
    phase += 111860.78f / period / 44100.f;
    phase -= std::floor(phase);
    return phase < .5f ? 1.f : -1.f;
  }
  static float psgNoise(Voice &v, float rate) {
    v.noisePhase += rate / 44100.f;
    while (v.noisePhase >= 1) {
      v.noisePhase -= 1;
      v.lfsr = (v.lfsr >> 1) ^ ((v.lfsr & 1) ? 0x14000u : 0u);
    }
    return v.lfsr & 1 ? 1.f : -1.f;
  }
  static float envelope(float a) {
    int level = int(clamp((1 - a) * 15, 0, 15));
    return level ? std::pow(10.f, (level - 15) / 10.f) : 0;
  }
  void render(float *out, int count) {
    unsigned r = read.load();
    while (r < write.load(std::memory_order_acquire)) {
      SoundEvent event = queue[r++ % 128];
      int k = event.type;
      for (auto &v : voices)
        if (v.age >= v.dur) {
          v = Voice{};
          v.type = k;
          v.source = event;
          v.dur = k == 3 ? 1.15f : k == 4 ? .48f : k == 9 ? .45f : k == 1 ? .14f : .24f;
          break;
        }
    }
    read.store(r, std::memory_order_release);
    int requested = int(clamp(level.load() - 1, 0, 5)), vol = volume.load();
    // A second sector advance during silence gets a complete morph interval
    // from the terrain's current shape, rather than jumping partway ahead.
    if (requested != pendingTrack && musicPhase == MusicPhase::Gap)
      phaseRemaining = transitionFrames;
    pendingTrack = requested;
    if (requested != currentTrack &&
        (musicPhase == MusicPhase::Playing || musicPhase == MusicPhase::FadeIn)) {
      fadeOutStart = transitionGain;
      musicPhase = MusicPhase::FadeOut;
      phaseRemaining = transitionFrames;
    }
    bool mus = music.load();
    constexpr float encounterFloor = .1f;
    const float encounterTarget = shipDucking.load() ? encounterFloor : 1.f;
    float wh = whistle.load(), missile = missileThreat.load();
    bool alarms = active.load() && sceneVisible.load(), ufo = ufoPresent.load();
    float musicGain = musicVolume.load() * .01f, effectGain = effectsVolume.load() * .01f;
    CameraView camera(viewAngle.load(), viewShake.load(), Terrain::unpack(viewTerrain.load()));
#ifdef __EMSCRIPTEN__
    if (useTrackedView) {
      camera = trackedView;
      camera.terrain = Terrain::unpack(viewTerrain.load());
    }
#endif
    bool scene = sceneVisible.load();
    bool audible[32];
    for (int v = 0; v < 32; v++)
      audible[v] =
          !voices[v].source.spatial ||
          (scene && camera.visible(voices[v].source.p + voices[v].source.velocity * voices[v].age,
                                   voices[v].source.radius));
    auto readTrack = [&](int id, float &left, float &right) {
      left = right = 0;
      if (id < 0 || !tracks[id].pcm || tracks[id].frames < 4410)
        return;
      Track &t = tracks[id];
      int p = t.position;
      left = t.pcm[p * 2] / 32768.f;
      right = t.pcm[p * 2 + 1] / 32768.f;
      constexpr int blend = 4410;
      if (p >= t.frames - blend) {
        int q = p - (t.frames - blend);
        float w = q / float(blend);
        left = left * (1 - w) + t.pcm[q * 2] / 32768.f * w;
        right = right * (1 - w) + t.pcm[q * 2 + 1] / 32768.f * w;
      }
      if (++t.position >= t.frames)
        t.position = blend;
    };
    for (int i = 0; i < count; i++, sample++) {
      float sec = sample / 44100.f, songL = 0, songR = 0;
      if (musicPhase != MusicPhase::Gap)
        readTrack(currentTrack, songL, songR);
      if (musicPhase == MusicPhase::Playing)
        transitionGain = 1;
      else if (musicPhase == MusicPhase::FadeOut) {
        transitionGain = fadeOutStart * phaseRemaining / transitionFrames;
        if (--phaseRemaining == 0) {
          musicPhase = MusicPhase::Gap;
          phaseRemaining = transitionFrames;
        }
      } else if (musicPhase == MusicPhase::Gap) {
        transitionGain = 0;
        if (--phaseRemaining == 0) {
          currentTrack = pendingTrack;
          tracks[currentTrack].position = 0;
          musicPhase = MusicPhase::FadeIn;
          phaseRemaining = transitionFrames;
        }
      } else {
        transitionGain = 1 - phaseRemaining / float(transitionFrames);
        if (--phaseRemaining == 0)
          musicPhase = MusicPhase::Playing;
      }
      songL *= transitionGain;
      songR *= transitionGain;
      if (!mus)
        songL = songR = 0;
      float fx = 0;
      bool hasSource = false;
      int voiceIndex = 0;
      for (auto &v : voices) {
        bool visible = audible[voiceIndex++];
        float voiceFx = 0;
        if (v.age >= v.dur)
          continue;
        float a = v.age / v.dur;
        float amp = envelope(a), f = 0, tone = 0, noise = 0;
        switch (v.type) {
        case 1: // Short, stepped cannon chirp.
          f = 1800 / (1 + std::floor(a * 10) * .38f);
          voiceFx += psgTone(v.phase, f) * amp * .27f;
          break;
        case 2: // Crunching meteor noise with a low PSG tone gate.
          tone = psgTone(v.phase, 180 - std::floor(a * 8) * 15);
          noise = psgNoise(v, 8500 - 6000 * a);
          voiceFx += ((tone > 0 && noise > 0) ? 1.f : -1.f) * amp * .31f;
          break;
        case 3: // Cannon loss: longer falling noise envelope.
          tone = psgTone(v.phase, 95 - 60 * a);
          noise = psgNoise(v, 11000 * std::pow(1 - a, 2) + 400);
          voiceFx += ((tone > 0 && noise > 0) ? 1.f : -1.f) * amp * .68f;
          break;
        case 4:
          voiceFx += psgTone(v.phase, int(a * 12) % 2 ? 440 : 330) * amp * .18f;
          break;
        case 5: {
          const float notes[] = {660, 880, 1320};
          voiceFx += psgTone(v.phase, notes[std::min(2, int(a * 3))]) * amp * .25f;
          break;
        }
        case 6: {
          const float notes[] = {440, 550, 660, 880};
          voiceFx += psgTone(v.phase, notes[std::min(3, int(a * 4))]) * amp * .23f;
          break;
        }
        case 8:
          f = a < .45f ? 1300 - 1200 * a : 750 - 500 * a;
          voiceFx += psgTone(v.phase, f) * amp * .31f * bombGain;
          break;
        case 9:
          voiceFx +=
              (psgNoise(v, 9500 - 8500 * a) * .40f + psgTone(v.phase, 270 - 210 * a) * .15f) * amp;
          break;
        case 10:
          voiceFx += (psgTone(v.phase, 900 - 720 * a) * .21f + psgNoise(v, 6000) * .13f) * amp;
          break;
        case 11:
          voiceFx += psgTone(v.phase, 480 - 230 * a) * amp * .20f;
          break;
        }
        if (visible) {
          fx += voiceFx;
          hasSource = true;
        }
        v.age += 1.f / 44100;
      }
      if (alarms && wh > 0)
        fx += psgTone(spinnerPhase, 220 + 1780 * wh) * .095f * bombGain;
      if (alarms && missile > 0) {
        float gate = std::fmod(sec * 6, 1) < .27f ? 1 : 0;
        fx += psgTone(missilePhase, 410 + 190 * missile) * gate * (.045f + .045f * missile);
      }
      if (alarms && ufo) {
        // Repeating stepped square-wave warble evokes a classic flying-saucer cue.
        // Keep the engine audible throughout visible flight, with a chattering pulse.
        static constexpr float flightPitch[16] = {300, 340, 390, 450, 520, 600, 690, 780,
                                                  780, 690, 600, 520, 450, 390, 340, 300};
        float pulse = std::fmod(sec * 8, 1) < .55f ? 1.f : .55f;
        fx += psgTone(ufoPhase, flightPitch[int(sec * 32) % 16]) * pulse * .18f;
      }
      hasSource = hasSource || (alarms && (wh > 0 || missile > 0 || ufo));
      if (!hasSource)
        fxDC = 0;
      fxDC += (fx - fxDC) * .002f;
      fx -= fxDC;
      // Temporary music-only envelope; user volume preferences are untouched.
      // One second down to 10%, then half a second back to the normal music volume.
      if (encounterGain > encounterTarget)
        encounterGain = std::max(encounterTarget, encounterGain - (1.f - encounterFloor) / 44100.f);
      else if (encounterGain < encounterTarget)
        encounterGain = std::min(encounterTarget, encounterGain + (1.f - encounterFloor) / 22050.f);
      float gain = vol * .01f;
      out[2 * i] =
          std::tanh((songL * .90f * musicGain * encounterGain + fx * .90f * effectGain) * gain);
      out[2 * i + 1] =
          std::tanh((songR * .90f * musicGain * encounterGain + fx * .90f * effectGain) * gain);
    }
    uint32_t terrainFrame = musicPhase == MusicPhase::FadeOut ? 0u
                            : musicPhase == MusicPhase::Gap   ? transitionFrames - phaseRemaining
                                                              : transitionFrames;
    terrainTimeline.store((uint32_t(pendingTrack + 1) << 16) | terrainFrame);
    if (recording)
      std::fwrite(out, sizeof(float), count * 2, recording);
  }

  ~Audio() {
    if (device)
      SDL_CloseAudioDevice(device);
    if (recording)
      std::fclose(recording);
    for (auto &t : tracks) {
#ifndef __EMSCRIPTEN__
      if (t.mapping) {
        munmap(t.mapping, t.mappingBytes);
        continue;
      }
#endif
      std::free(t.pcm);
    }
  }
};
} // namespace db
