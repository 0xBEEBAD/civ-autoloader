# Third-party notices

This repository contains original code and pinned third-party components.
Each component keeps its own license.

## Runtime components

| Component | License | Source |
|---|---|---|
| CivSwitch | MIT | `third_party/CivSwitch` |
| CivSwitchFS | MIT | `third_party/CivSwitchFS` |
| kaiHEN | GPL-2.0-only | `third_party/kaihen` |
| libkaihen | ISC | `third_party/kaihen/libkaihen` |
| ounce-hbloader | ISC | `packages/sdcard/vendor/game/main` |
| ounce-hbmenu | ISC | `packages/sdcard/vendor/hbmenu.nro` |
| ftpd | Unlicense | `packages/sdcard/vendor/plugins/ftpd.nso` |
| libnx | ISC | devkitPro |
| libounce | ISC | devkitPro / ounce-devkit |
| CivSwitchLDN | GPL-3.0-only | ESP32 LDN transport (`main/civ7.c`, `main/main.c`); `packages/transport/linux/ldn` |
| Kinnay LDN | GPL-3.0 | key derivation / advertisement format |
| pokeldn | AGPL-3.0 | ESP32 `wpa_funcs` association patch |
| xiaolizi23/civswitch-ldn-nx | GPL-3.0-only | `packages/transport/nro` (Switch 1 homebrew entry) |
| IZCIVSwitch | GPL-2.0-or-later | `packages/transport/linux/bash/network-panel.sh`, `ldn/panel_network.py`, the `engine.json` shape |

## Build tools

| Component | License |
|---|---|
| ESP-IDF | Apache-2.0 |
| esptool | GPL-2.0-or-later |
| Node.js / npm / esbuild / terser | MIT / ISC / Apache-2.0 |
| devkitPro portlibs | see `packages/sdcard/licenses/` |

## License boundary

- `kaiHEN` is GPL-2.0-only. The SD card package includes kaiHEN binaries, so
  distributing that package requires the corresponding source. The source is
  `third_party/kaihen` and the `SeanHeuc/kaihen` fork.
- The ESP32 **LDN transport** is ported from CivSwitchLDN (GPL-3.0-only),
  Kinnay LDN (GPL-3.0) and pokeldn (AGPL-3.0). The firmware package therefore
  cannot be distributed as MIT-only while those files are present. Choose one:
  1. license `packages/firmware/esp32` under GPL-3.0 (and comply with AGPL for
     the pokeldn-derived part), or
  2. replace the ported LDN code with a clean-room implementation.
- The JavaScript payloads (`CivSwitch`, `CivSwitchFS`, the overlay) are MIT and
  are not linked with the GPL firmware code.
- `packages/transport/nro` is a GPL-3.0-only derivative of
  `xiaolizi23/civswitch-ldn-nx`, which itself derives from CivSwitchLDN. It
  stays in its own directory with its own license file.
- `packages/transport/linux` vendors CivSwitchLDN's `ldn/` (GPL-3.0-only) and
  adapts IZCIVSwitch's radio handling (GPL-2.0-or-later). It also stays in its
  own directory.

The full license texts shipped with the SD card are in
`packages/sdcard/licenses/`.
