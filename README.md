# civ-autoloader

Autoloader for **Civilization VII** on Switch 2.

A host joins the game's local-wireless lobby, delivers the CivSwitch userland
loader, and installs the **autoloader save**. On the next `Load Game` the save
runs CivSwitchFS, prepares kaiHEN, and boots it.

## Status

| Entry | Host | Status |
|---|---|---|
| ESP32-S3 | fully offline board | **tested** (Switch 2, 23.0.1) |
| Linux PC | Linux + Wi-Fi radio | **not tested yet** |
| Switch 1 NRO | second console | **not tested yet** |

Only the ESP32 path has been run on hardware. The Linux and Switch 1 NRO
entries build and share the same payload and save, but have not completed an
install yet. See [`packages/transport/README.md`](packages/transport/README.md)
for each entry's requirements and caveats.

## What you need

| Item | Notes |
|---|---|
| Console | Switch 2, firmware **23.0.1** |
| Game | Civilization VII **1.0.0 physical cartridge** |
| Host | an ESP32-S3 (16 MB flash), **or** Linux with a Wi-Fi radio, **or** a Switch 1 with homebrew |
| Keys | your own `prod.keys` (not needed for the Switch 1 NRO) |
| SD card | **microSD Express**, formatted **exFAT** (recommended on an updated Switch 2) or **FAT32** for smaller cards |

The SD card is required for kaiHEN (`hbmenu.nro` and `kaihen/`). Without it,
the FS stage still runs, but kaiHEN preparation stops with an on-screen
message.

## Layout

```text
packages/
  firmware/esp32/    ESP-IDF firmware (tested entry)
  transport/
    linux/           Linux PC entry (untested)
    nro/             Switch 1 homebrew entry (untested)
    build-payload.js shared chat payload builder
    compress.js      shared chat compressor
  sdcard/            SD card staging and pinned prebuilt binaries
  userland/          notes for the CivSwitch userland loader
  fs/                notes for the CivSwitchFS stage
  kaihen/            notes for the kaiHEN installer stage
  orchestrator/      notes for the FS -> kaiHEN stage pipeline
third_party/
  CivSwitch/         fork: game-side userland loader
  CivSwitchFS/       fork: FS elevation + autoloader overlay
  kaihen/            fork: HEN / RO / bootstrap
docs/                architecture, build, flashing, compatibility
```

## 1. Prepare the SD card

```sh
python3 packages/sdcard/stage.py
```

Copy the **contents** of `packages/sdcard/contents/` to the root of the SD
card. The card must contain:

```text
/hbmenu.nro
/kaihen/bootstrap.bin
/kaihen/bin/game/main
/kaihen/bin/hen/main
/kaihen/bin/ro/main
/kaihen/hen-options.txt
/licenses/...
```

`kaihen/plugins/ftpd.nso` is optional (FTP server on port 5000).

## 2. Build the save blobs

All three entries deliver the same two blobs. Build them once:

```sh
git submodule update --init --recursive

# Build dependencies (once)
(cd third_party/CivSwitchFS && npm ci)
(cd third_party/CivSwitch/PicoSwitch && npm install)
(cd third_party/CivSwitch/installer-creator && npm install)

# Put your own prod.keys here (gitignored)
cp /path/to/prod.keys packages/firmware/esp32/prod.keys

cd packages/firmware/esp32
source /path/to/esp-idf/export.sh          # ESP32 entry only
python3 tools/build_and_flash.py --keys prod.keys
```

Outputs:

```text
blobs/blob_tpl.bin    save template
blobs/blob_apply.bin  save installer + compressed CFS stage
```

The storage partition is 4 MB.

kaiHEN only arms for **Switch 2 games**: titles whose program ID has the `0x04`
family prefix (`0x04xxxxxxxxxxxxxx`). A Switch 1 game (`0x0100...`) runs from its
own process and is not a supported target for this chain.

## 3a. Install with the ESP32 (tested)

```sh
cd packages/firmware/esp32
python3 tools/build_and_flash.py --keys prod.keys \
  --port /dev/cu.usbmodemXXXX --flash
```

1. Insert the SD card into the console.
2. Start Civ VII.
3. `MULTIPLAYER -> WIRELESS -> CREATE GAME -> HOST LOBBY`.
4. Power the board (USB power is enough).
5. Wait for `PERSIST INSTALL DONE - restart the game, then Load Game`.
6. Close Civ VII completely, reopen it, choose **Load Game**, and load
   `XSTRIG.Civ7Save`.

## 3b. Install with a Linux PC (not tested yet)

Requirements: real Linux (not WSL), one Wi-Fi radio with nl80211,
NetworkManager, Python 3.12+, `libcap2-bin`.

```sh
cd packages/transport/linux
npm install                 # creates ldn/.venv, asks sudo for setcap
cp /path/to/prod.keys prod.keys
node index.js --verbose
```

Keep the Civ VII wireless lobby open. The script temporarily releases the
Wi-Fi radio, joins the lobby over LDN, sends the payload, streams the save over
UDP 9027, and restores networking when it finishes. Then close Civ VII,
reopen it, and `Load Game`.

See [`packages/transport/linux/README.md`](packages/transport/linux/README.md).

## 3c. Install with a Switch 1 NRO

Requirements: Switch 1 homebrew, `ldn_mitm` removed and the console rebooted,
both consoles close together (local wireless only), Civ VII 1.0.0 on the
Switch 2.

```sh
cd packages/transport/nro
export DEVKITPRO=/opt/devkitpro
make
```

Copy `civ-autoloader-nx.nro` to `/sdcard/switch/` and launch it from the
Album. The app shows two install options:

```text
civ-autoloader NX by sean v0.10
press ZR: install CivSwitch
press R: install autoloader (kaiHEN)
```

| Button | Save | What `Load Game` does next |
|---|---|---|
| **ZR** | standard CivSwitch save | CivSwitch loader is back on TCP `:9026` |
| **R** | autoloader save | CivSwitchFS -> prepare kaiHEN -> boot hbmenu |

With no lobby joined, pressing either button scans immediately and, when only
one Civ VII room is visible, joins it and installs without further input. With
several rooms visible, pick one with Up/Down and press **A**.

After `Save installer: Persistent trigger save installed`, close Civ VII on the
Switch 2, reopen it, and choose `Load Game`.

**Warning:** this replaces every Civilization VII single-player save.

See [`packages/transport/nro/README.md`](packages/transport/nro/README.md).

## 4. After the install

The save runs:

```text
CivSwitchFS -> prepare kaiHEN -> boot kaiHEN -> hbmenu
```

When it finishes, the console shows:

```text
AUTOLOADER: kaiHEN installation complete. You can close Civ7 now.
```

The save is offline: it does not wait for a network after the autoloader.
The same-session CFS stage is intentionally not used because running CFS
immediately after the save install can freeze the console. The save is the
hand-off point.

## Fallback

If the persistent autoloader save causes FS canary failures, the standard
CivSwitch save is a fallback: use `make_install_blobs.py` and the `save`
stage. The default build keeps the persistent save because it is the
one-click path.

## Dev / manual mode

For manual testing, set `devMode` to `true` in
`third_party/CivSwitchFS/src/core/autoloader.mjs`, rebuild, and reflash:

```js
const CONFIG = {
    modules: ["fs", "kaihen"],
    kaihenAuto: true,
    devMode: true
};
```

The autoloader then only prepares kaiHEN and keeps the FS bridge alive; it
prints `dev mode; kaiHEN boot skipped`. Use the bridge's manual commands to
inspect or boot HEN.

The kaiHEN SD log is **off by default**. To enable it, uncomment `log-sd` in
`/kaihen/hen-options.txt`.

## Troubleshooting

- The console shows the autoloader progress on screen.
- The ESP32 serial log is available with `tools/wait-log.py`.
- No SD log is written before kaiHEN starts.
- If the SD card is missing, the FS stage still completes and the console
  reports which kaiHEN files are missing.

## Standing on the shoulders of giants

This project would not exist without the work below. Thank you.

| Project | What we used | License |
|---|---|---|
| [CivSwitch](https://github.com/Gezine/CivSwitch) | userland exploit, Remote JS Loader, save installer design, chat compression | MIT |
| [CivSwitchFS](https://github.com/ItsNerisma/CivSwitchFS) | FS elevation, persistent save codec/installer | MIT |
| [kaiHEN](https://github.com/kurumuz/kaihen) | HEN, RO, bootstrap | GPL-2.0-only; libkaihen ISC |
| [CivSwitchLDN](https://github.com/ItsNerisma/CivSwitchLDN) | LDN / Civ VII protocol flow and constants | GPL-3.0-only |
| [Kinnay LDN](https://github.com/kinnay/LDN) | key derivation, advertisement/auth format | GPL-3.0 |
| [pokeldn](https://github.com/Decryptu/pokeldn) | ESP32 `wpa_funcs` association patch | AGPL-3.0 |
| [ns2jb](https://gitlab.com/kurumuz/ns2jb) | FS path-normalizer / reserved-gap research | no license file in the reference |
| [civ7-fs-kaihen](https://github.com/s8n/civ7-fs-kaihen) | arm flow, thread scheduling, FS reference | reference only; no license file present |
| CivSwitch ns2jb port (Slapah) | FS acquisition idea and reference shape checks | MIT (per CivSwitchFS notices) |
| [nx-hbloader](https://github.com/switchbrew/nx-hbloader) | NS1 loader reference | ISC |
| [nx-hbmenu](https://github.com/switchbrew/nx-hbmenu) | NS1 menu reference | ISC |
| [ounce-hbloader](https://github.com/ouncebrew/ounce-hbloader) | NS2 `game/main` | ISC |
| [ounce-hbmenu](https://github.com/ouncebrew/ounce-hbmenu) | NS2 `hbmenu.nro` | ISC |
| [OunceShell](https://github.com/kurumuz/ounceshell) | FTP plugin source | GPL-3.0 |
| [ftpd](https://github.com/kurumuz/ounceshell) | FTP server (`switch2/third_party/ftpd`) | Unlicense |
| [libnx](https://github.com/switchbrew/libnx) | Switch runtime | ISC |
| [libounce](https://github.com/ouncebrew/ounce-devkit) | Switch 2 runtime | ISC |
| [ESP-IDF](https://github.com/espressif/esp-idf) | ESP32-S3 firmware framework | Apache-2.0 |
| [devkitPro](https://devkitpro.org/) | toolchain and portlibs | various; see `packages/sdcard/licenses/` |
| [Atmosphere](https://github.com/Atmosphere-NX/Atmosphere) | FS source reference | GPL-2.0 |
| [civswitch-kit](https://gitee.com/mza1010/civswitch-kit) | reference / control implementation | GPL-2.0 (with Gezine MIT notice) |
| [SwitchBrew](https://switchbrew.org/) | system documentation | documentation reference |

`ns2jb` and `civ7-fs-kaihen` are research references; no source tree from them
is shipped here. The full license texts for the shipped components are in
`packages/sdcard/licenses/` and `THIRD_PARTY_NOTICES.md`.

## License

The original JavaScript payloads and tooling are MIT. The ESP32 firmware's LDN
transport is ported from GPL-3.0-only / AGPL-3.0 projects, so
`packages/firmware/esp32` is distributed under GPL-3.0 (and the pokeldn-derived
part under AGPL-3.0). Third-party components keep their own licenses; see
`THIRD_PARTY_NOTICES.md` and `packages/sdcard/licenses/`.