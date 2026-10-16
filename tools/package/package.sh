#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
make music-cache
python3 - <<'PY'
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
root=Path.cwd()
files={
 'port/Domeblast 3D.sh':'Domeblast 3D.sh',
 'build/domeblast3d.aarch64':'domeblast3d/domeblast3d.aarch64',
 'assets/font.bmp':'domeblast3d/assets/font.bmp',
 'assets/FONT-LICENSE.txt':'domeblast3d/assets/FONT-LICENSE.txt',
 'assets/ports/screenshot.png':'domeblast3d/screenshot.png',
 'LICENSE':'domeblast3d/LICENSE',
 'LICENSE-ASSETS':'domeblast3d/LICENSE-ASSETS',
}
with ZipFile(root/'build/Domeblast3D-Handhelds.zip','w',ZIP_DEFLATED) as z:
    for source,target in files.items(): z.write(root/source,target)
    for folder in ('music','music-cache','textures','title'):
        for p in (root/'assets'/folder).iterdir():
            if p.is_file():z.write(p,'domeblast3d/assets/'+folder+'/'+p.name)
print(root/'build/Domeblast3D-Handhelds.zip')
PY
