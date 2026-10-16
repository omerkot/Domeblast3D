#!/usr/bin/env python3
"""Add/update this native game's shortcut and artwork with Steam fully closed.

Uses only the standard library. Unrelated binary KeyValues fields are retained;
unsupported field types fail before any write. Never edits compatibility config.
"""
import argparse
import os
import re
from pathlib import Path
import shutil
import struct
import time
import zlib


def native_resolution(text, appid):
    """Edit one text KeyValues leaf, retaining every other byte of localconfig."""
    tokens = list(re.finditer(r'//[^\n]*|"(?:\\.|[^"\\])*"|[{}]|[^\s{}"]+', text))
    tokens = [t for t in tokens if not t.group().startswith('//')]
    cursor = 0

    def obj(nested=False):
        nonlocal cursor
        values = []
        while cursor < len(tokens):
            key = tokens[cursor]
            cursor += 1
            if key.group() == '}':
                if not nested:
                    raise ValueError('Unbalanced localconfig')
                return values, key.start()
            if cursor >= len(tokens):
                raise ValueError('Truncated localconfig')
            value = tokens[cursor]
            cursor += 1
            if value.group() == '{':
                children, end = obj(True)
                values.append((key.group().strip('"'), children, end, None))
            else:
                values.append((key.group().strip('"'), None, value.start(), value.end()))
        if nested:
            raise ValueError('Unclosed localconfig')
        return values, len(text)

    values, end = obj()
    path = ['UserLocalConfigStore', 'Software', 'Valve', 'Steam', 'shortcuts', str(appid),
            'ResolutionOverride2']
    for depth, name in enumerate(path):
        entry = next((v for v in values if v[0].lower() == name.lower()), None)
        if entry is None:
            addition = '"ResolutionOverride2"\t\t"Native"\n'
            for part in reversed(path[depth:-1]):
                addition = f'"{part}"\n{{\n{addition}}}\n'
            return text[:end] + '\n' + addition + text[end:]
        _, children, start, stop = entry
        if depth == len(path) - 1:
            if children is not None:
                raise ValueError('Resolution setting is an object')
            return text[:start] + '"Native"' + text[stop:]
        if children is None:
            raise ValueError(f'Expected localconfig object: {name}')
        values, end = children, start


def decode(data):
    pos = 0

    def cstring():
        nonlocal pos
        end = data.index(b'\0', pos)
        value = data[pos:end]
        pos = end + 1
        return value

    def obj():
        nonlocal pos
        result = []
        while True:
            kind = data[pos]
            pos += 1
            if kind == 8:
                return result
            key = cstring()
            if kind == 0:
                value = obj()
            elif kind == 1:
                value = cstring()
            elif kind in (2, 3, 4, 6, 7, 10):
                size = 8 if kind in (7, 10) else 4
                value = data[pos:pos + size]
                if len(value) != size:
                    raise ValueError('Truncated KeyValues field')
                pos += size
            else:
                raise ValueError(f'Unsupported KeyValues type {kind}; file unchanged')
            result.append((kind, key, value))

    result = obj()
    if pos != len(data):
        raise ValueError('Trailing KeyValues data; file unchanged')
    return result


def encode(fields):
    out = bytearray()
    for kind, key, value in fields:
        out += bytes([kind]) + key + b'\0'
        out += encode(value) if kind == 0 else value + (b'\0' if kind == 1 else b'')
    return bytes(out) + b'\x08'


def field(fields, name):
    return next((value for _, key, value in fields if key.lower() == name.lower()), None)


def string(name, value):
    return (1, name.encode(), value.encode())


def integer(name, value):
    return (2, name.encode(), struct.pack('<I', value))


def register(config, game):
    path = config / 'shortcuts.vdf'
    original = path.read_bytes() if path.exists() else b'\0shortcuts\0\x08\x08'
    tree = decode(original)
    entries = field(tree, b'shortcuts')
    if not isinstance(entries, list):
        raise ValueError('Missing shortcuts object; file unchanged')
    exe = '"' + str(game / 'Domeblast 3D.sh') + '"'
    appid = zlib.crc32((exe + 'Domeblast 3D').encode()) | 0x80000000
    existing = next((entry for entry in entries if entry[0] == 0 and
                     field(entry[2], b'Exe') == exe.encode()), None)
    if existing:
        stored = field(existing[2], b'appid')
        if stored and len(stored) == 4:
            appid = struct.unpack('<I', stored)[0]
        values = existing[2]
    else:
        numbers = [int(k) for _, k, _ in entries if k.isdigit()]
        values = []
        entries.append((0, str(max(numbers, default=-1) + 1).encode(), values))
    updates = [integer('appid', appid), string('AppName', 'Domeblast 3D'),
               string('Exe', exe), string('StartDir', '"' + str(game) + '"'),
               string('icon', str(game / 'assets/steam/icon.png')),
               string('LaunchOptions', ''), integer('IsHidden', 0),
               integer('AllowDesktopConfig', 1), integer('AllowOverlay', 1)]
    for update in updates:
        index = next((i for i, v in enumerate(values) if v[1].lower() == update[1].lower()), None)
        if index is None:
            values.append(update)
        else:
            values[index] = update
    if field(values, b'tags') is None:
        values.append((0, b'tags', [string('0', 'Native Linux')]))
    result = encode(tree)
    assert encode(decode(result)) == result
    local_path = config / 'localconfig.vdf'
    local_original = local_path.read_text() if local_path.exists() else ''
    local_updated = native_resolution(local_original, appid)
    artwork = {'library_600x900.png': f'{appid}p.png', 'header.png': f'{appid}.png',
               'library_hero.png': f'{appid}_hero.png', 'logo.png': f'{appid}_logo.png',
               'icon.png': f'{appid}_icon.png'}
    for source in artwork:
        if not (game / 'assets/steam' / source).is_file():
            raise ValueError(f'Missing artwork: {source}')
    config.mkdir(parents=True, exist_ok=True)
    stamp = str(time.time_ns())
    if path.exists():
        shutil.copy2(path, config / f'shortcuts.vdf.domeblast3d-{stamp}.bak')
    if local_path.exists():
        shutil.copy2(local_path, config / f'localconfig.vdf.domeblast3d-{stamp}.bak')
    grid = config / 'grid'
    grid.mkdir(exist_ok=True)
    for source, dest in artwork.items():
        target = grid / dest
        if target.exists():
            shutil.copy2(target, grid / f'{dest}.{stamp}.bak')
        shutil.copy2(game / 'assets/steam' / source, target)
    temporary = path.with_name('shortcuts.vdf.domeblast3d.tmp')
    temporary.write_bytes(result)
    temporary.replace(path)
    local_temporary = local_path.with_name('localconfig.vdf.domeblast3d.tmp')
    local_temporary.write_text(local_updated)
    local_temporary.replace(local_path)
    return appid


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game_dir', type=Path)
    parser.add_argument('--steam-root', type=Path, default=Path.home() / '.local/share/Steam')
    parser.add_argument('--user-id', help='Steam account folder; required if several accounts exist')
    args = parser.parse_args()
    for proc in Path('/proc').glob('[0-9]*'):
        try:
            if proc.stat().st_uid == os.getuid() and (proc / 'comm').read_text().strip() == 'steam':
                parser.error('Exit Steam in Desktop Mode, then run this installer again.')
        except (OSError, ProcessLookupError):
            pass
    users = args.steam_root / 'userdata'
    profiles = [p for p in users.iterdir() if p.is_dir() and p.name.isdigit() and p.name != '0']
    if args.user_id:
        profiles = [p for p in profiles if p.name == args.user_id]
    if len(profiles) != 1:
        parser.error('Select one Steam account with --user-id (log in to Steam once first).')
    game = args.game_dir.resolve()
    if not (game / 'Domeblast 3D.sh').is_file():
        parser.error('Game launcher not found')
    appid = register(profiles[0] / 'config', game)
    print(f'Registered Domeblast 3D with five artwork assets; appid={appid}')


if __name__ == '__main__':
    main()
