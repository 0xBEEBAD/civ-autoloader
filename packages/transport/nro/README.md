# Switch 1 NRO transport

Delivers the install from a **second console** running homebrew, so no PC and
no `prod.keys` are needed.

**Not tested yet.** See [`../README.md`](../README.md) for the status of every
entry point.

Based on [xiaolizi23/civswitch-ldn-nx](https://github.com/xiaolizi23/civswitch-ldn-nx)
(GPL-3.0-only), retargeted to install our **persistent autoloader save**
instead of the standard trigger save.

## Layout

```text
Makefile                 devkitPro/libnx build
source/main.c            UI, scan, join, controls
source/game_probe.c      Civ7 handshake + chat delivery
source/installer.c       UDP/TCP delivery of the save
source/ldn_guard.c       blocks when ldn_mitm is resident
source/chat_payload.h    generated: core.js + UDP receiver
source/installer_payload.h generated: persist bootstrap + adapter
data/save_template.bin   generated: pristine save template
tools/generate_assets.py regenerates the three generated files
```

## Requirements

- Switch 1 able to run `.nro` homebrew
- Switch 2 with Civilization VII **1.0.0**, Switch 1 edition cartridge
- `ldn_mitm` (or any `ldn:u` MITM) **removed**, console rebooted
- both consoles on the same Wi-Fi/LAN for the save upload
- launch from the **Album**; a title override only works when overriding
  Civilization VII itself

## Build

```sh
export DEVKITPRO=/opt/devkitpro
cd packages/transport/nro

# assets are checked in
make

# regenerate assets from the current payload first
python3 tools/generate_assets.py
make clean all
```

Output: `civ-autoloader-nx.nro`.

`tools/generate_assets.py` runs the shared
[`../build-payload.js`](../build-payload.js) so the chat payload matches the
ESP32 and Linux entries, then wraps the CivSwitchFS persist bootstrap and
appends a small adapter:

```text
stage window.__tpl chunks into stageChunk()
call installFromTemplate()
log "INSTALL DONE"
```

## Use

Copy the NRO to the SD card:

```text
/sdcard/switch/civ-autoloader-nx.nro
```

On the Switch 2, open `MULTIPLAYER -> WIRELESS -> CREATE GAME -> HOST LOBBY`.
On the Switch 1, launch the NRO from the Album.

| Button | Action |
|---|---|
| Up / Down | select a scanned lobby |
| A | join |
| X | rescan |
| B | leave |
| + | exit |

After **JOINED**, install the save, wait for `INSTALL DONE`, then on the
Switch 2 close Civ VII, reopen it, and `Load Game`.

**Warning:** the installer replaces every Civilization VII single-player save.
Back up first.

## Notes

- The NRO keeps the upstream project's guard against `ldn_mitm` and its
  launch-mode check; both block with instructions instead of scanning.
- The save upload uses the normal Wi-Fi/LAN connection, not LDN. Both consoles
  must reconnect to the same network, with automatic join enabled.
