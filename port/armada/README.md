# Domeblast 3D — Odin 3 / ArmadaOS

Native ARM64 Linux edition for the AYN Odin 3 running ArmadaOS. It is the same
SDL2/GLES2 executable as the ROCKNIX and muOS handheld edition
(`src/main.cpp`), with the same gameplay, single-screen HUD, effects and Six
Skies soundtrack, added to Steam as a native Linux game.

## Install

Extract `Domeblast3D-Odin3-ArmadaOS.zip` on the console. Enter Desktop Mode,
exit Steam fully, open a terminal in the extracted folder, and run:

```sh
bash install.sh
```

Reopen Steam and select **Library → Non-Steam → Domeblast 3D**. The installer
adds the portrait cover, horizontal capsule, hero, transparent logo and icon,
and sets this game's Steam resolution to Native (preserving the global setting).
Use the normal gamepad Steam Input layout. Leave forced compatibility tools
unchecked: this is a native Linux executable. If more than one Steam account
has logged in, run `bash install.sh --user-id ACCOUNT_FOLDER_NUMBER`.

Installs to `~/Games/domeblast3d`. Saves and logs use
`${XDG_DATA_HOME:-~/.local/share}/domeblast3d/`. Updates back up the previous
installation and Steam shortcuts; unrelated Steam entries are preserved.
To transfer an RG34XXSP save, exit the game and copy its `save.dat` into this
state directory. Run the launcher directly in Desktop Mode if desired.

## Display and controls

The game uses the actual SDL fullscreen drawable every frame, including the
Odin 3's **1920×1080 landscape native resolution**. World rendering,
postprocessing and HUD all use that framebuffer; menu proportions remain
uniform. Set Steam's per-game resolution to Native if Gamescope is configured
to offer a lower resolution. Only one window is created, also when docked.

| Control | Action |
|---|---|
| Left stick or D-pad | Move |
| Right stick | Aim |
| R1 / A | Fire |
| L2 / R2 | Orbit the camera |
| L3 | Precision movement |
| A / Start | Start a run |
| Start | Pause / resume |
| B | Back one level (cancel a dialog, resume from pause) |
| X | During a run: confirm returning to the title. On the title: confirm quitting |
| Select | Music on/off |
| Y | Autofire |
| R3 on title | Fire rate |
| D-pad on title / in pause | Choose sector and volume / adjust audio levels |

Controller mappings come from SDL and Steam Input. Losing focus, minimizing
the game or disconnecting the controller pauses play.

## Build

On the development host, `./tools/package/package-armada.sh` builds in a
Debian 12 ARM64 Docker container, prepares music caches and
creates the ZIP. Target dependencies: SDL2, GLESv2, glibc and libstdc++ supplied
by Armada. The launcher uses Armada's system GPU/audio libraries.

`--verify --seconds 35` captures the title and all six sectors without modifying
saves. Captures are written in the game directory; diagnostics are in the
state directory's `log.txt`.

Platform integration checked against Armada upstream revision
`0ef1fdb7851ba4bf9ca4105c8433ad9767837f32` on 2026-09-13:
https://github.com/armada-os/armada

## License

Domeblast 3D is open source. Code is licensed under the MIT License and original artwork, music and documentation under CC BY 4.0. See `LICENSE`, `LICENSE-ASSETS` and the project README for credits and third-party licenses.
