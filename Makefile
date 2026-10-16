CXX ?= g++
CXXFLAGS = -O2 -std=c++17 -Wall -Wextra -Wno-missing-field-initializers
CPPFLAGS += $(shell pkg-config --cflags sdl2 glesv2)
LDLIBS += $(shell pkg-config --libs sdl2 glesv2) -pthread
all: build/domeblast3d.aarch64
build/domeblast3d.aarch64: src/main.cpp src/game.hpp src/render.hpp src/audio.hpp src/music_cache.hpp third_party/stb_vorbis.c third_party/stb_image.h src/material_shaders.hpp src/geometry.hpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) src/main.cpp -o $@ $(LDLIBS)
.PHONY: music-cache
music-cache: assets/music-cache/.ready
build/cache_music: tools/build/cache_music.cpp src/music_cache.hpp third_party/stb_vorbis.c
	mkdir -p build
	$(CXX) $(CXXFLAGS) tools/build/cache_music.cpp -o $@
assets/music-cache/.ready: tools/build/cache_music.cpp src/music_cache.hpp third_party/stb_vorbis.c $(wildcard assets/music/sector*.ogg) | build/cache_music
	./build/cache_music
	touch $@
