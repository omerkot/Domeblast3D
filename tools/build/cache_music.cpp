// Decode the original Ogg score at build/install time, with the same decoder
// and sample format used by the game. No resampling or recomposition occurs.
#include "../../src/music_cache.hpp"
#include "../../third_party/stb_vorbis.c"
#include <filesystem>
#include <string>
int main() {
  std::filesystem::create_directories("assets/music-cache");
  for (int i = 1; i <= 6; ++i) {
    std::string source = "assets/music/sector" + std::to_string(i) + ".ogg";
    std::string output = "assets/music-cache/sector" + std::to_string(i) + ".pcm";
    db::MusicCacheHeader header;
    if (!db::musicFingerprint(source.c_str(), header.sourceBytes, header.sourceHash))
      return 1;
    short *pcm = nullptr;
    int channels = 0, rate = 0;
    int frames = stb_vorbis_decode_filename(source.c_str(), &channels, &rate, &pcm);
    if (frames < 4410 || channels != 2 || rate != 44100) {
      std::fprintf(stderr, "Cannot cache %s\n", source.c_str());
      std::free(pcm);
      return 1;
    }
    header.frames = frames;
    std::string temporary = output + ".incoming";
    FILE *file = std::fopen(temporary.c_str(), "wb");
    if (!file) {
      std::free(pcm);
      return 1;
    }
    bool ok = std::fwrite(&header, sizeof(header), 1, file) == 1 &&
              std::fwrite(pcm, sizeof(short) * 2, frames, file) == size_t(frames);
    ok = std::fclose(file) == 0 && ok;
    std::free(pcm);
    if (!ok || std::rename(temporary.c_str(), output.c_str()) != 0)
      return 1;
    std::printf("Cached sector %d: %.1f seconds, %.1f MiB\n", i, frames / 44100.f,
                (sizeof(header) + uint64_t(frames) * 4) / (1024.f * 1024.f));
  }
}
