#!/usr/bin/env python3
"""Update only Domeblast 3D's entry; call with EmulationStation stopped."""
from pathlib import Path
import xml.etree.ElementTree as ET
import shutil
from datetime import datetime
ports=Path('/roms/ports')
path=ports/'gamelist.xml'
if path.exists():
    shutil.copy2(path,path.with_name('gamelist.xml.domeblast3d-'+datetime.now().strftime('%Y%m%d-%H%M%S')+'.bak'))
    tree=ET.parse(path)
else:
    tree=ET.ElementTree(ET.Element('gameList'))
root=tree.getroot()
entry=next((g for g in root.findall('game') if g.findtext('path')=='./Domeblast 3D.sh'),None)
if entry is None: entry=ET.SubElement(root,'game')
fields={'path':'./Domeblast 3D.sh','name':'Domeblast 3D','desc':'360-degree arena defense. Drive across the arena in two directions, fire through 3D space, and rotate the camera with L2/R2. Six Skies original soundtrack: six newly composed, fully arranged pieces. A/Start launches; left stick drives; right stick aims/fires; Y toggles autofire; Start pauses.','image':'./domeblast3d/screenshot.png','genre':'Shooter','developer':'Domeblast 3D','players':'1','releasedate':'20260905T000000'}
for name,value in fields.items():
    element=entry.find(name)
    if element is None: element=ET.SubElement(entry,name)
    element.text=value
ET.indent(tree,space='  ')
tmp=path.with_suffix('.xml.tmp')
tree.write(tmp,encoding='utf-8',xml_declaration=True)
tmp.replace(path)
print('Registered Domeblast 3D; preserved all other Ports entries.')
