# Linux LDN transport

Delivers the autoloader save from a Linux PC over local wireless.

**Not tested yet.** See [`../README.md`](../README.md) for the status of every
entry point.

## Layout

```text
index.js                 driver: blobs -> bash/network-panel.sh -> ldn/main.py
engine.json              engine declaration (platforms, ports, requirements)
build-payload.js         shared (../build-payload.js)
compress.js              shared (../compress.js)
bash/network-panel.sh    release/restore only the selected Wi-Fi radio
bash/setup.sh            Python venv + setcap
ldn/                     CivSwitchLDN's LDN client (GPL-3.0-only)
ldn/panel_network.py     keeps the temporary LDN interface out of
                         NetworkManager's view
```

`ldn/` is vendored from
[ItsNerisma/CivSwitchLDN](https://github.com/ItsNerisma/CivSwitchLDN)
(GPL-3.0-only) and uses Kinnay's `ldn==0.0.21` (GPL-3.0). It keeps its own
license terms.

## Setup

```sh
npm install            # venv + pip + sudo setcap
cp /path/to/prod.keys prod.keys
```

Requirements: real Linux, a Wi-Fi radio with nl80211, NetworkManager, Python
3.12+, and `libcap2-bin`.

## Run

Build the blobs once (from the repository root):

```sh
cd packages/firmware/esp32
python3 tools/build_and_flash.py --keys prod.keys
```

Then:

```sh
cd packages/transport/linux
node index.js --verbose
```

The console must be on `MULTIPLAYER -> WIRELESS -> CREATE GAME -> HOST LOBBY`.

## What it sends

```text
chat messages   core.js + UDP receiver (built by ../build-payload.js)
UDP :9027       A<template chunks>:<apply chunks>:<cfs>:<start>:<ip>
                then T<i>/J<i> chunks; the console NAKs with t<i>/j<i>
                and answers I:OK / I:ERR when the save is written
```

The ESP32 firmware and the NRO send the same two blobs; this client's
`--installer-template` / `--installer-apply` files are the raw base64 payloads
from `blob_tpl.bin` / `blob_apply.bin`.

## Differences from upstream CivSwitchLDN

- Sends our persistent autoloader save instead of the standard trigger save.
- Header carries the 5-field form; the receiver accepts both 3- and 5-field.
- Waits for `I:OK` / `I:ERR`.

## Troubleshooting

- `Radio phy0 not found` — check `/sys/class/ieee80211`.
- `Needs root or CAP_NET_ADMIN + CAP_NET_RAW` — rerun `npm install`, or run
  `sudo setcap cap_net_admin,cap_net_raw=ep ldn/.venv/bin/python-ldn`.
- Wi-Fi does not come back — reconnect manually; the trap prints a warning.
  Ethernet stays up the whole time, so a remote shell survives either way.
- `A previous LDN interface still exists` — a stale `civ7-autoloader-ldn` was
  left behind. Restore Wi-Fi (or reboot) before starting another run.
- `Multiple LDN networks found` — pass `--host <mac>`.
