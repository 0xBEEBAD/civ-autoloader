# Migration review

Date: 2026-11-15

This is the copy-only migration from the research workspace. Nothing was
committed or pushed. Every copied file is listed below.

## Submodules

| Path | Fork | Base commit |
|---|---|---|
| `third_party/CivSwitch` | `SeanHeuc/CivSwitch` | `2a9a359` |
| `third_party/CivSwitchFS` | `SeanHeuc/CivSwitchFS` | `cdc3397` |
| `third_party/kaihen` | `SeanHeuc/kaihen` | `5ac7725` |

The forks are at the upstream base. The local working trees carry the changes
listed below until they are committed and pushed.

## CivSwitchFS

Source: `reference/_external/CivSwitchFS`

Modified:

| File | Diff |
|---|---|
| `src/core.js` | +3/-1: import `autoloader.mjs`, pass `onElevated` |
| `src/core/config.mjs` | +1: `skipPidPromotion: false` |
| `src/core/promotion.mjs` | +23: skip PID promotion, `closeUserStorage`, `releaseRetainedEndpoint` |
| `src/core/visuals.mjs` | +37/-4: log tail, 80px bottom padding, rAF scroll |

New:

| File | Lines |
|---|---:|
| `src/core/autoloader.mjs` | 41 |
| `src/core/autoloader-fs.mjs` | 3370 |
| `src/core/autoloader-kaihen.mjs` | 386 |

## kaiHEN

Source: `kaihen`

Modified:

| File | Diff |
|---|---|
| `common/code_mount_request.hpp` | +13/-1 |
| `common/loader_gate.hpp` | +13 |
| `common/sd_writer.cpp` | +123 |
| `common/sd_writer.hpp` | +25 |
| `hen/source/server.cpp` | +14 |
| `ro/source/control.cpp` | +7 |
| `ro/source/control.h` | +4 |
| `ro/source/fs_memory.cpp` | +301 |
| `ro/source/mapping.cpp` | +5 |

New:

| File | Lines |
|---|---:|
| `hen/source/log.cpp` | 419 |
| `hen/source/log.hpp` | 26 |

## ESP32 firmware

Source: `CivSwitch/esp32-ldn-civ7`

Copied:

```text
CMakeLists.txt
README.md
Screenshot_1.jpg
partitions.csv
sdkconfig.defaults
main/CMakeLists.txt
main/bridge.c
main/bridge.h
main/civ7.c
main/civ7.h
main/installer.c
main/installer.h
main/main.c
main/private_wifi.h
tools/build_and_flash.py
tools/console.py
tools/loaderjs-test.js
tools/make_cfs_blob.py
tools/make_install_blobs.py
tools/make_keys.py
tools/make_payload.py
tools/prepare_cfs.py
tools/serial-log.py
tools/udp_loader.js
tools/wait-log.py
tools/wait_ready.py
```

Excluded (generated, secret, or moved elsewhere):

```text
build/
blobs/
prod.keys
cfs-overlay/      -> copied into third_party/CivSwitchFS/src/core/
sd-card/          -> copied into packages/sdcard/
sdkconfig
```

Path changes:

- `build_and_flash.py`: `CIV` -> `third_party/CivSwitch`, `--cfs` defaults to
  `third_party/CivSwitchFS`, overlay stamping uses the CFS checkout.
- `make_payload.py`: `CIV` -> `third_party/CivSwitch`.
- `make_install_blobs.py`: `CIV` -> `third_party/CivSwitch`.
- `prepare_cfs.py`: only checks the hook; the overlay files are already in the
  CFS checkout.
- `make_cfs_blob.py`: `--dir` defaults to `third_party/CivSwitchFS`.

## SD card

Source: `CivSwitch/esp32-ldn-civ7/sd-card`

Copied:

```text
hbmenu.nro              -> packages/sdcard/vendor/hbmenu.nro
kaihen/bin/game/main    -> packages/sdcard/vendor/game/main
kaihen/bin/hen/main     -> packages/sdcard/vendor/hen/main
kaihen/bin/ro/main      -> packages/sdcard/vendor/ro/main
kaihen/bootstrap.bin    -> packages/sdcard/vendor/bootstrap.bin
kaihen/hen-options.txt  -> packages/sdcard/vendor/hen-options.txt
kaihen/plugins/ftpd.nso -> packages/sdcard/vendor/plugins/ftpd.nso
licenses/               -> packages/sdcard/licenses/
```

Excluded:

```text
switch/ounceshell.nro   (not part of the core release)
```

## Build verification (migration snapshot)

At the copy-only migration, the build produced:

```text
blob_apply.bin      38808   identical to the old blob
blob_tpl.bin      1546312   identical to the old blob
blob_loaderjs.bin    1440   identical to the old blob
blob_cfs.bin       450612   core identical except BUILD_UTC
build/esp32-ldn-civ7.bin 703568
```

At migration time the CFS core was byte-identical to the old one after
normalizing `BUILD_UTC`. The persistent autoloader save was added afterwards
and now defaults to `kaihenAuto:true`; see `docs/build.md` for the current
blobs.

## Post-migration changes

- kaiHEN: removed the TCP log port. Only the SD file sink remains, and it is
  off by default (`hen-options.txt`).
- autoloader: `kaihenAuto` is true by default for the one-click path.
  `devMode` (or `globalThis.__CIVSWITCH_DEV__`) keeps the manual bridge.
- ESP32: the default install is the persistent autoloader save. The save runs
  CFS on the next `Load Game`; the same-session CFS stage is not used because
  it can freeze the console. The storage partition is 4 MB.
- The standard CivSwitch save remains as a fallback if the larger save
  degrades the FS canary.
- ESP32: verbose LDN/auth/chunk logs are `ESP_LOGD` and off at the default
  INFO log level.

## License note

The ESP32 LDN transport is ported from CivSwitchLDN (GPL-3.0-only), Kinnay LDN
(GPL-3.0) and pokeldn (AGPL-3.0). The firmware package cannot be MIT-only.
See `THIRD_PARTY_NOTICES.md`.
