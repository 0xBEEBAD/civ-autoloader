# ESP32 LDN installer

An ESP32-S3 that joins the console's **Civ VII wireless lobby** and injects the
CivSwitch payload over **local wireless** — no PC, no USB HID typing, no Wi-Fi
or LAN access, and **no system-language or controller-layout changes**.

With the payload it also carries the full persistent installer (the 1 MB save
template), so one run does everything: exploit → installer → back to the normal
TCP loader on `:9026`.

When the install finishes the board switches its radio to an access point
called **N-Hole**, which serves a Nintendo-aware DNS sinkhole and a mini HTTP
responder (the nanodns policy, on the board). Hold **BOOT** for 1.5 s to leave
the hotspot and run the LDN flow again.

## Why another board firmware?

`esp32-usb-pad` (the "net-pad") types the payload over a **USB HID keyboard**
and exposes a Wi-Fi web UI. It works for any game that needs autotyping, and it
is the right choice for future titles.

This firmware is **Civ VII specific** and is the recommended path today:

| | `esp32-usb-pad` (net-pad) | `esp32-ldn-civ7` (this) |
|---|---|---|
| Transport | USB HID typing | Civ VII wireless lobby (LDN) |
| Console setup | English/USB keyboard language, layout changes | none |
| PC required | for the web UI / installer upload | **no** |
| Network required | Wi-Fi (console + board) | **none** (LDN only) |
| Secrets | — | your own `prod.keys` (derived keys baked at build time) |
| Speed | ~7 minutes of typing | ~10-15 seconds |
| Works for other games | yes (autotyping) | no (Civ VII only) |

The LDN path matters when the console cannot reach the internet: a heavily
blocked DNS setup (e.g. 90DNS) makes the console refuse to stay online, so
anything that needs the console's LAN is unreliable. This firmware never leaves
the local-wireless link.

## Requirements

| Item | Value |
|---|---|
| Board | ESP32-S3, **16 MB flash** (4 MB works if you shrink the storage partition) |
| Toolchain | ESP-IDF **v6.1** |
| Host tools | Node.js (for `terser`), Python 3.10+ |
| Console | Switch 2, Civ VII **1.0.0 physical cartridge** (Switch 1 edition) |
| Keys | your own `prod.keys`, dumped from your Switch 1|
| Game state | `MULTIPLAYER → WIRELESS → CREATE GAME → HOST LOBBY` |

`prod.keys` is **never committed** — `keys.h` is generated at build time and is
gitignored together with the payloads and blobs.

## One-click build + flash

```sh
cd packages/firmware/esp32
source /path/to/esp-idf/export.sh      # idf.py on PATH
python3 tools/build_and_flash.py \
    --keys prod.keys \
    --port /dev/cu.usbmodem1234 \
    --flash
```

`--cfs` defaults to `third_party/CivSwitchFS` and `--core` defaults to
`third_party/CivSwitch/core.js`.

What it does:

1. `main/keys.h` from your `prod.keys` (`tools/make_keys.py`);
2. `main/chat_payload.h` from `third_party/CivSwitch/core.js` + the UDP
   installer channel (`tools/make_payload.py`);
3. `blobs/blob_tpl.bin` (save template, base64) and `blobs/blob_apply.bin`
   (persistent save installer + compressed CFS stage, base64) via
   `tools/make_persist_blobs.py`;
4. `idf.py build`;
5. `esptool write-flash`: bootloader + partition table + app + blobs.

Options: `--erase` (full chip erase first), `--no-build` (regenerate assets
only), `--core path` (use a different `core.js`), `--port $ESPPORT`.

After flashing, **press RESET** on the board (esptool's reset can leave the chip
in ROM download mode).

## Run it

The board has one loop:

```text
power on -> wait 1 s -> LDN flow (scan, join, install)
         -> N-Hole hotspot up
         -> hold BOOT 1.5 s -> LDN flow again -> N-Hole ...
```

1. Start Civ VII and open `MULTIPLAYER → WIRELESS → CREATE GAME → HOST LOBBY`.
2. Power the board (USB power is enough; it does not need to be plugged into
   the console). The LDN flow starts by itself after one second.
3. The board scans, joins, authenticates and sends the payload automatically.
The original CivSwitch save is installed first. After that the board streams
CivSwitchFS over the same LDN channel and the console evaluates it in RAM; the
CivSwitchFS save installer is **not** used. If the CFS stage is absent the
firmware behaves as before.

4. The console shows:

```text
civ-autoloader
CivSwitch 1.0 by Gezine
UDP PAYLOAD CHANNEL :9027
UDP header tpl=1406 js=219 cfs=0 s=0 (nak)
RUNNING PERSIST (...)
PERSIST INSTALL DONE - restart the game, then Load Game
PERSIST RETURNED
```

5. The board prints `N-Hole up: ssid=N-Hole ip=192.168.4.1` and parks there.
6. Restart the game, choose **Load Game**, and load `XSTRIG.Civ7Save`.
   The save runs CivSwitchFS, prepares kaiHEN, and boots it.

Each install needs a fresh lobby. After the save is installed, later runs only
need Load Game.

Nothing happens if no lobby is open? The board keeps rescanning every ~11 s
until it finds one, and **BOOT** abandons the scan and goes straight to the
hotspot.

While `wait-log.py` is running, typing `nhole` in its terminal does the same as
the button, which is handy on a board whose BOOT button is hard to reach.

## N-Hole

After the LDN flow the radio becomes an open access point:

| Item | Value |
|---|---|
| SSID | `N-Hole` (open, no password) |
| Board address | `192.168.4.1` |
| Services | DNS on UDP `:53`, HTTP on TCP `:80` |
| Clients | up to 4 |

Join it from the console or a phone and use the board as the DNS server. The
policy is the one nanodns uses on the console:

| Query | Answer |
|---|---|
| `ctest.cdn.nintendo.net`, `ctest.srv.nintendo.net`, `conntest.nintendowifi.net` (and subdomains) | `192.168.4.1`; `:80` answers `200 OK` |
| any other `*.nintendo.*` / `*.nintendowifi.net` name | `0.0.0.0` (A), `::` (AAAA), or no data |
| everything else | `NXDOMAIN` |

The served hosts get the headers the console's connectivity check wants:

```text
HTTP/1.1 200 OK
X-Organization: Nintendo
Cache-Control: no-store
Content-Type: text/plain

ok
```

Two differences from the console plugin, both because the board is a standalone
hotspot:

- **No uplink.** nanodns forwards every non-Nintendo name to an upstream
  resolver, because the console keeps its normal internet connection. The
  board is AP-only, so forwarding would stall on an unreachable resolver and
  then time out; non-Nintendo names get `NXDOMAIN` immediately instead. This
  means N-Hole is not a general-purpose internet hotspot.
- **The served address is the AP's own address**, printed at start-up. The
  console plugin answers `127.0.0.1` because it runs on the console itself.

`N-Hole down: q=... served=... blocked=... nx=... http=.../...` is printed when
the hotspot stops, so a BOOT re-run leaves a summary on the serial log.

## SD card preparation

The ready-to-copy files live in `packages/sdcard/`. Run
`python3 packages/sdcard/stage.py`, then copy the **contents** of
`packages/sdcard/contents/` to the root of an **exFAT or FAT32** microSD card,
then insert the card into the console before starting:

```text
/hbmenu.nro
/kaihen/bootstrap.bin
/kaihen/bin/ro/main
/kaihen/bin/hen/main
/kaihen/bin/game/main
/licenses/...
```

The save checks `/hbmenu.nro`, `/kaihen/bootstrap.bin`,
`/kaihen/bin/hen/main`, and `/kaihen/bin/game/main`. If a file is missing, the
console reports it and keeps the FS bridge alive. It does not write to the
card before kaiHEN starts.

## How it works

```text
scan LDN advertisements (channels 1/6/11)
  → derive advertise_key from prod.keys (AES-ECB + SHA-256)
  → decrypt the advertisement (AES-CTR, SHA-256 verified) → server_random
  → data key (CCMP) = KDF(server_random + Civ VII passphrase)
  → associate to the hidden SSID (no 4-way handshake, keys installed manually)
  → LDN authentication (challenge HMAC-SHA256)
  → IP from the host's advertisement (169.254.x.y)
  → ARP warmup, FireCONN → NetWelcome
  → 19 chat messages (core.js + UDP receiver), 0.2 s apart
  → console evals the payload and binds UDP :9027
  → board streams the installer (template + apply JS) over LDN in one pass
  → console NAKs only the missing chunks; the board resends just those
  → console starts the installer as soon as the last chunk arrives
  → console writes XSTRIG.Civ7Save and commits
  → board swaps the radio to N-Hole (SoftAP + DNS sinkhole + HTTP)
  → restart, Load Game → CivSwitchFS → kaiHEN → hbmenu
```

## Layout

```text
packages/firmware/esp32/
  CMakeLists.txt
  partitions.csv           app 1 MB + 2 MB storage for the installer blobs
  sdkconfig.defaults
  main/
    main.c                 scan/associate/auth/IP, drives civ7 + installer; BOOT/run loop
    civ7.c/.h              Civ VII LDN protocol (transport, FireCONN, NetChat)
    installer.c/.h         streams the installer over UDP :9027
    nhole.c/.h             N-Hole: SoftAP, DNS sinkhole, mini HTTP responder
    private_wifi.h         ESP-IDF v6.1 wpa_funcs ABI (no 4-way handshake)
  tools/
    build_and_flash.py     one-click
    make_keys.py           prod.keys -> main/keys.h (gitignored)
    make_payload.py        third_party/CivSwitch/core.js -> main/chat_payload.h (gitignored)
    make_persist_blobs.py  persistent save -> blobs/ (default)
    make_cfs_blob.py       optional current-session CFS blob
    make_install_blobs.py  standard save fallback
    udp_loader.js          the UDP channel spliced into core.js
  blobs/                   generated
```

## Serial debug log

The LDN board mirrors the console-side loader log back over the same LDN UDP
channel. The board prints those lines on its USB-Serial/JTAG console as:

```text
console: RUNNING PERSIST (...)
console: PERSIST INSTALL DONE - restart the game, then Load Game
console: AUTOLOADER: BUILD: ...
console: KAIHEN PREP: required SD files are present
console: AUTOLOADER: HEN: boot sequence complete
```

Read it from the PC while the console is running:

```sh
python3 tools/serial-log.py --port /dev/cu.usbmodem2101
```

To arm the log before entering the lobby and stop automatically at the end of a
run:

```sh
python3 tools/wait-log.py --port /dev/cu.usbmodem2101
```

`wait-log.py` exits on `PERSIST INSTALL DONE` or
`AUTOLOADER: HEN: boot sequence complete` (success), or on the failure lines.
With `--board-ready` it also exits on `N-Hole up:`, i.e. as soon as the board
side is done, before you reach Load Game.
It writes the full run to `/tmp/autoloader-*.log`.
It reopens the port automatically when the board resets and USB-Serial/JTAG
re-enumerates, so start it before pressing RESET.

Use **one** serial reader at a time. Two `serial-log.py` processes on the same
`/dev/cu.usbmodem*` split/lose bytes; `wait-log.py` warns before opening if
`lsof` shows another reader.

This is the debug path when there is no network access; the board's own scanner
and the console payload logs share the same serial stream.

## Troubleshooting

| Symptom | Cause |
|---|---|
| `assoc attempt N did not associate` | lobby closed / wrong channel; re-create the lobby |
| `LDN authentication failed` | wrong `prod.keys` or the lobby changed |
| `no FireRPLY/NetWelcome` | the console left the lobby; restart the game |
| `UDP incomplete missT=...` | the board is re-sending; check `rx=` growth |
| `binding failed` / `udp bind failed` | port 9027 already in use in the game |
| `N-Hole failed to start` | radio did not leave LDN cleanly; hold BOOT 1.5 s to retry |
| Console shows nothing | the chat XSS did not render; check the lobby chat |
| Board not detected by esptool | hold BOOT, tap RESET, release BOOT |
| Board ignores BOOT after boot | BOOT is held at reset -> ROM download mode; tap RESET to boot the firmware |

## Persistent autoloader save (Load Game)

`--cfs DIR` defaults to `third_party/CivSwitchFS`. The build packages:

```text
blobs/blob_tpl.bin    save template
blobs/blob_apply.bin  save installer + compressed CFS stage
```

The LDN UDP channel sends the template as `T<i>` chunks and the installer as
`J<i>` chunks. The console writes the autoloader save. Close the game, reopen
it, choose **Load Game**, and load `XSTRIG.Civ7Save`; the save runs:

```text
CivSwitchFS -> prepare kaiHEN -> boot kaiHEN
```

When it finishes, the console shows:

```text
AUTOLOADER: kaiHEN installation complete. You can close Civ7 now.
```

The save is offline: it does not fall through to the TCP loader's
`network down` wait loop. The same-session CFS stage is intentionally not
used because it can freeze the console. If the persistent save causes FS
canary failures, the standard save is a fallback: use
`make_install_blobs.py` and the `save` stage.

The SD card must be inserted before Load Game. It must contain `hbmenu.nro`
and the `kaihen/` files from `packages/sdcard/contents/`. Without the card,
the FS stage still completes and the console reports the missing kaiHEN
files; no SD log is written before kaiHEN starts.

`kaihenAuto` is true by default. For manual testing, set `devMode` to `true`
in `third_party/CivSwitchFS/src/core/autoloader.mjs`, rebuild, and reflash.

## Notes

- `prod.keys` and everything derived from it (`main/keys.h`) stay offline.
- The installer blob contains the pristine save template from the CivSwitchFS
  persist build; it is regenerated from the repo, not copied around.
- The board never joins your Wi-Fi in this firmware; the station side is only
  used for the LDN association. After the run it is an AP called `N-Hole`.
- N-Hole is not an internet hotspot: it has no uplink, so only the Nintendo
  names above are answered (see [N-Hole](#n-hole)).

## Credits & license

This firmware ports the local-wireless path from these projects. Their licenses
apply to the ported parts; check them before redistributing a binary:

| Project | What was used | License |
|---|---|---|
| [CivSwitch](https://github.com/Gezine/CivSwitch) | payload, save template, installer design | MIT |
| [CivSwitchLDN](https://github.com/ItsNerisma/CivSwitchLDN) | LDN/Civ VII protocol flow, constants | GPL-3.0-only |
| [Kinnay LDN](https://github.com/kinnay/LDN) | key derivation, advertisement/auth format | GPL-3.0 |
| [pokeldn](https://github.com/Decryptu/pokeldn) | ESP32 `wpa_funcs` patch for handshake-less association | AGPL-3.0 |

Because the LDN transport is ported from GPL-3.0/AGPL-3.0 projects, this
firmware package is distributed under **GPL-3.0** (and the pokeldn-derived part
under **AGPL-3.0**), not the repository's MIT license. The JavaScript payloads
remain MIT.

`prod.keys` is never part of this repository; delete `main/keys.h`, `blobs/` and
`build/` before sharing a tree that was built on your own console.
