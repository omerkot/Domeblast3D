#!/bin/bash
# One native ARM64 build; SDL selects the screens and their drawable resolution.
PORTDIR="$(cd "$(dirname "$0")" && pwd)"
GAMEDIR="$PORTDIR/domeblast3d"
if [ -d /opt/muos ]; then
  # muOS keeps launchers under ROMS/Ports and game data under ports.
  for base in "${PORTDIR%/ROMS/Ports}" /mnt/union /mnt/sdcard /mnt/mmc; do
    if [ -x "$base/ports/domeblast3d/domeblast3d.aarch64" ]; then
      GAMEDIR="$base/ports/domeblast3d"
      break
    fi
  done
  export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run}"
  export SDL_GAMECONTROLLERCONFIG_FILE=/usr/lib/gamecontrollerdb.txt
  if [ -r /opt/muos/device/config/sdl/name ]; then
    controller_name=$(cat /opt/muos/device/config/sdl/name)
    export SDL_GAMECONTROLLERCONFIG="$(grep -F ",$controller_name," "$SDL_GAMECONTROLLERCONFIG_FILE")"
  fi
  export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-alsa}"
  # Let muOS's SDL select its native Mali framebuffer driver and panel orientation.
  if [ -e /run/muos/system/foreground_process ]; then
    printf '%s' domeblast3d.aarch64 > /run/muos/system/foreground_process
  fi
elif [ -d /storage/.config ] && command -v swaymsg >/dev/null 2>&1; then
  export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/var/run/0-runtime-dir}"
  export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-1}"
  export SWAYSOCK="${SWAYSOCK:-$XDG_RUNTIME_DIR/sway-ipc.0.sock}"
  export SDL_VIDEODRIVER=wayland
  export SDL_AUDIODRIVER=pulseaudio
  export SDL_VIDEO_DISPLAY_PRIORITY=DSI-2,DSI-1
  export MALI_DEFAULT_DISPLAY=wayland
  if [ -f /storage/.config/SDL-GameControllerDB/gamecontrollerdb.txt ]; then
    export SDL_GAMECONTROLLERCONFIG="$(grep "retrogame_joypad" /storage/.config/SDL-GameControllerDB/gamecontrollerdb.txt | sed -e "s/a:b0/a:b1/g" -e "s/b:b1/b:b0/g" -e "s/x:b2/x:b3/g" -e "s/y:b3/y:b2/g")"
  fi
  swaymsg -q 'for_window [title="^Domeblast 3D$"] border none, fullscreen enable, move window to output DSI-2' 2>/dev/null || true
  swaymsg -q 'for_window [title="^Domeblast 3D Secondary$"] border none, fullscreen enable, move window to output DSI-1' 2>/dev/null || true
fi
export SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS=0
cd "$GAMEDIR" || exit 1
./domeblast3d.aarch64 "$@" > log.txt 2>&1
exit "$?"
