#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
if ! command -v em++ >/dev/null; then
  if [[ -n "${EMSDK:-}" && -f "$EMSDK/emsdk_env.sh" ]]; then source "$EMSDK/emsdk_env.sh" >/dev/null 2>&1;
  else printf '%s\n' 'Install Emscripten 6.0.9 and source emsdk_env.sh, or set EMSDK.' >&2; exit 1; fi
fi
mkdir -p dist
em++ src/web_main.cpp -O2 -std=c++17 -fexceptions --use-port=sdl2 \
  -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sALLOW_MEMORY_GROWTH=1 \
  -sINITIAL_MEMORY=67108864 -sMAXIMUM_MEMORY=536870912 -sSTACK_SIZE=2097152 \
  -sENVIRONMENT=web -sEXIT_RUNTIME=0 -sASSERTIONS=1 \
  -sEXPORTED_RUNTIME_METHODS='["UTF8ToString","HEAPF32"]' \
  --preload-file assets/textures/material-atlas.png \
  --preload-file assets/title/domeblast-3d.png --preload-file assets/font.bmp \
  --preload-file assets/music -o dist/domeblast.js
cp web/index.html web/style.css web/app.js web/input.mjs web/xr.mjs web/xr-math.mjs web/controller-mapping.mjs web/controller-setup.mjs web/touch-controls.mjs web/browser-display.mjs web/manifest.webmanifest dist/
cp LICENSE LICENSE-ASSETS dist/
cp assets/steam/icon.png dist/app-icon.png
cp assets/title/domeblast-3d.png dist/title.png
printf '%s\n' 'Web build ready in dist/'
