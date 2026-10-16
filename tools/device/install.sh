#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
device_host="${1:?Usage: install.sh <device-ip-or-hostname>}"
[[ "$device_host" =~ ^[a-zA-Z0-9.-]+$ ]] || { echo 'Invalid hostname'; exit 1; }
remote="root@$device_host"
ssh_cmd=(ssh -o StrictHostKeyChecking=accept-new)
scp_cmd=(scp -o StrictHostKeyChecking=accept-new)
if [ -n "${DEVICE_PASSWORD:-${RGDS_PASSWORD:-}}" ]; then
  export SSHPASS="${DEVICE_PASSWORD:-$RGDS_PASSWORD}"
  ssh_cmd=(sshpass -e "${ssh_cmd[@]}")
  scp_cmd=(sshpass -e "${scp_cmd[@]}")
fi
[ -f build/domeblast3d.aarch64 ] || { echo 'Run tools/build/build.sh first.'; exit 1; }
make music-cache
staging_dir=$(mktemp -d)
trap 'rm -rf "$staging_dir"' EXIT
mkdir -p "$staging_dir/domeblast3d/assets"
cp build/domeblast3d.aarch64 "$staging_dir/domeblast3d/"
cp LICENSE LICENSE-ASSETS "$staging_dir/domeblast3d/"
cp assets/font.bmp assets/FONT-LICENSE.txt "$staging_dir/domeblast3d/assets/"
cp -R assets/music assets/music-cache assets/textures assets/title "$staging_dir/domeblast3d/assets/"
cp 'port/Domeblast 3D.sh' "$staging_dir/"
[ ! -f assets/ports/screenshot.png ] || cp assets/ports/screenshot.png "$staging_dir/domeblast3d/screenshot.png"
tar -czf "$staging_dir/package.tar.gz" -C "$staging_dir" 'Domeblast 3D.sh' domeblast3d
"${scp_cmd[@]}" "$staging_dir/package.tar.gz" "$remote:/tmp/domeblast3d-package.tar.gz"
"${scp_cmd[@]}" tools/device/register_port.py "$remote:/tmp/domeblast3d-register.py"
"${ssh_cmd[@]}" "$remote" 'bash -s' <<'REMOTE'
set -euo pipefail
if pidof domeblast3d.aarch64 >/dev/null; then
  echo 'Domeblast 3D is running. Exit the game before updating.'; exit 1
fi
ports=/roms/ports
launcher_dir="$ports"
backup_dir=/storage/.local/share/domeblast3d-backups
muos=0
if [ -d /opt/muos ]; then
  muos=1
  storage=/mnt/mmc
  [ ! -d /mnt/sdcard/ROMS ] || storage=/mnt/sdcard
  ports="$storage/ports"
  launcher_dir="$storage/ROMS/Ports"
  backup_dir=/mnt/mmc/MUOS/backup/domeblast3d
fi
stamp=$(date +%Y%m%d-%H%M%S)
mkdir -p "$ports" "$launcher_dir"
if [ -d "$ports/domeblast3d" ]; then
  mkdir -p "$backup_dir"
  tar -cf - -C "$ports" domeblast3d | gzip > "$backup_dir/$stamp.tar.gz"
  [ ! -f "$launcher_dir/Domeblast 3D.sh" ] || cp "$launcher_dir/Domeblast 3D.sh" "$backup_dir/$stamp-launcher.sh"
fi
was_active=0
if [ "$muos" = 0 ] && systemctl is-active --quiet essway.service; then
  was_active=1
  systemctl stop essway.service
fi
trap 'if [ "$was_active" = 1 ]; then systemctl start essway.service; fi' EXIT
gzip -dc /tmp/domeblast3d-package.tar.gz | tar -xf - -C "$ports"
if [ "$launcher_dir" != "$ports" ]; then
  mv "$ports/Domeblast 3D.sh" "$launcher_dir/Domeblast 3D.sh"
fi
chmod +x "$launcher_dir/Domeblast 3D.sh" "$ports/domeblast3d/domeblast3d.aarch64"
[ "$muos" = 1 ] || python3 /tmp/domeblast3d-register.py
sync
rm -f /tmp/domeblast3d-package.tar.gz /tmp/domeblast3d-register.py
printf 'Installed under Ports: Domeblast 3D\n'
REMOTE
