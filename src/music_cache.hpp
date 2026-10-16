#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
namespace db {
// Native PCM cache v1: little-endian stereo signed 16-bit samples at 44100 Hz.
// The source fingerprint prevents stale caches from replacing an updated score.
struct MusicCacheHeader {
  char magic[8] = {'D', 'B', 'P', 'C', 'M', '0', '1', '\0'};
  uint32_t rate = 44100, channels = 2;
  uint64_t frames = 0, sourceBytes = 0, sourceHash = 0;
};
static_assert(sizeof(MusicCacheHeader) == 40, "Unexpected PCM cache header layout");
inline bool musicFingerprint(const char *path, uint64_t &bytes, uint64_t &hash) {
  FILE *file = std::fopen(path, "rb");
  if (!file)
    return false;
  bytes = 0;
  hash = UINT64_C(14695981039346656037);
  unsigned char buffer[32768];
  size_t count;
  while ((count = std::fread(buffer, 1, sizeof(buffer), file)) != 0) {
    bytes += count;
    for (size_t i = 0; i < count; ++i) {
      hash ^= buffer[i];
      hash *= UINT64_C(1099511628211);
    }
  }
  bool ok = !std::ferror(file);
  std::fclose(file);
  return ok;
}
} // namespace db
