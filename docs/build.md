# Build

## Prerequisites

- ESP-IDF **v6.1** (with `idf.py` and `esptool`)
- Node.js 20+ and npm
- Python 3
- devkitPro + libnx + libounce (only for rebuilding kaiHEN)

## Submodules

```sh
git submodule update --init --recursive
```

The submodule working trees currently carry the local overlay/patches. They
must be committed and pushed to the forks before a fresh clone can build
without them.

## Node dependencies

```sh
(cd third_party/CivSwitchFS && npm ci)
(cd third_party/CivSwitch/PicoSwitch && npm install)
(cd third_party/CivSwitch/installer-creator && npm install)
```

## Keys

Put your own `prod.keys` at:

```text
packages/firmware/esp32/prod.keys
```

It is gitignored. Never commit it.

## Firmware

```sh
source /path/to/esp-idf/export.sh
cd packages/firmware/esp32
python3 tools/build_and_flash.py --keys prod.keys
```

Build only by default. Outputs:

```text
blobs/blob_tpl.bin       save template
blobs/blob_apply.bin     save installer + compressed CFS stage
build/esp32-ldn-civ7.bin
```

The default install is the persistent autoloader save. The save runs CFS on
the next `Load Game`; the same-session CFS stage is not used because it can
freeze the console.

If the persistent save causes FS canary failures, the standard save is a
fallback: use `make_install_blobs.py` and the `save` stage.

## Manual mode

Set `devMode` to `true` in
`third_party/CivSwitchFS/src/core/autoloader.mjs` and rebuild. The autoloader
only prepares kaiHEN and keeps the FS bridge alive.

## SD card

```sh
python3 packages/sdcard/stage.py
```

Copy `packages/sdcard/contents/*` to the root of the SD card.

## kaiHEN (optional)

```sh
export DEVKITPRO=/path/to/devkitpro
export LIBOUNCE=$DEVKITPRO/libounce
make -C third_party/kaihen
```

Copy the new `hen/build/main`, `ro/build/main` and
`bootstrap/build/bootstrap.bin` into `packages/sdcard/vendor/`, then re-run
`stage.py`.
