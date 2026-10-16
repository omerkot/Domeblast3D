#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
./tools/build/build.sh
make music-cache
python3 - <<'PY'
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
root = Path.cwd()
dest = root / 'build/Domeblast3D-Odin3-ArmadaOS.zip'
with ZipFile(dest, 'w', ZIP_DEFLATED) as archive:
    files = {'build/domeblast3d.aarch64': 'domeblast3d/domeblast3d.aarch64',
             'port/armada/Domeblast 3D.sh': 'domeblast3d/Domeblast 3D.sh',
             'port/armada/install.sh': 'install.sh',
             'tools/device/register_steam.py': 'register_steam.py',
             'port/armada/README.md': 'README.md',
             'LICENSE': 'LICENSE',
             'LICENSE-ASSETS': 'LICENSE-ASSETS'}
    for source, target in files.items():
        archive.write(root / source, target)
    for folder in ('music', 'music-cache', 'textures', 'title', 'steam'):
        for file in (root / 'assets' / folder).iterdir():
            if file.is_file() and not file.name.startswith('.'):
                archive.write(file, 'domeblast3d/assets/' + folder + '/' + file.name)
    for name in ('font.bmp', 'FONT-LICENSE.txt'):
        archive.write(root / 'assets' / name, 'domeblast3d/assets/' + name)
print(dest)
PY
