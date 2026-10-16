#!/usr/bin/env bash
set -euo pipefail
game_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
state_dir="${XDG_DATA_HOME:-$HOME/.local/share}/domeblast3d"
mkdir -p "$state_dir"
export DOMEBLAST_SAVE_PATH="${DOMEBLAST_SAVE_PATH:-$state_dir/save.dat}"
export SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS=0
# Use Armada's native GPU/audio libraries, outside Steam's bundled runtime.
unset LD_LIBRARY_PATH LD_PRELOAD
# Gamescope supplies DISPLAY; KDE can supply either X11 or Wayland. SDL chooses
# the current session's backend. Never force Mali, a DSI output, or RGDS mappings.
cd "$game_dir"
exec ./domeblast3d.aarch64 --single "$@" > "$state_dir/log.txt" 2>&1
