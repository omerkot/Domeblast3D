#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
[[ $(uname -m) == aarch64 ]] || { echo 'This package requires ARM64 Linux.'; exit 1; }
if pgrep -u "$(id -u)" -x steam >/dev/null; then
  echo 'In Desktop Mode, exit Steam, then run this installer again.'; exit 1
fi
if pgrep -u "$(id -u)" -f '/domeblast3d[.]aarch64' >/dev/null; then
  echo 'Exit Domeblast 3D before updating.'; exit 1
fi
game_dir="$HOME/Games/domeblast3d"
if [[ -d "$game_dir" ]]; then
  backup_dir="${XDG_DATA_HOME:-$HOME/.local/share}/domeblast3d/backups"
  mkdir -p "$backup_dir"
  tar -czf "$backup_dir/install-$(date +%Y%m%d-%H%M%S).tar.gz" -C "$HOME/Games" domeblast3d
fi
mkdir -p "$game_dir"
cp -R domeblast3d/. "$game_dir/"
chmod +x "$game_dir/Domeblast 3D.sh" "$game_dir/domeblast3d.aarch64"
python3 register_steam.py "$game_dir" "$@"
echo 'Installation complete. Open Steam → Library → Non-Steam → Domeblast 3D.'
