# Transport entry points

The same save and the same payload run from three different delivery paths.
All of them end with the console showing:

```text
AUTOLOADER: kaiHEN installation complete. You can close Civ7 now.
```

| Entry | Host | Status |
|---|---|---|
| ESP32-S3 board | fully offline | **tested** (Switch 2, 23.0.1) |
| Linux PC | Linux + Wi-Fi radio | **not tested yet** |
| Switch 1 NRO | second console | **not tested yet** |

Only the ESP32 path has been run on hardware so far. The Linux and NRO paths
build and share the same protocol code, but nobody has completed an install
with them.

## Shared pieces

```text
packages/transport/build-payload.js   core.js + UDP receiver -> chat lines
packages/transport/compress.js        terser + deflateRaw + base25 chunking
packages/firmware/esp32/tools/udp_loader.js   UDP :9027 receiver and stages
```

Every entry point delivers the same two blobs:

```text
blob_tpl.bin    the persistent autoloader save template
blob_apply.bin  the save installer (CivSwitchFS persist bootstrap)
```

Build them once:

```sh
cd packages/firmware/esp32
python3 tools/build_and_flash.py --keys prod.keys
```

That produces `blobs/blob_tpl.bin` and `blobs/blob_apply.bin`, plus the ESP32
firmware image.

## ESP32 (tested)

```sh
cd packages/firmware/esp32
python3 tools/build_and_flash.py --keys prod.keys \
  --port /dev/cu.usbmodemXXXX --flash
```

Then `MULTIPLAYER -> WIRELESS -> CREATE GAME -> HOST LOBBY`, power the board,
wait for `PERSIST INSTALL DONE`, close the game, reopen, `Load Game`.

## Linux (not tested yet)

Requirements:

- a **real Linux** system (WSL is not supported);
- one Wi-Fi radio with direct nl80211 access;
- NetworkManager (`nmcli`);
- Python 3.12+ with `venv`;
- `libcap2-bin` for `setcap`.

```sh
cd packages/transport/linux
npm install          # creates ldn/.venv and grants the Python helper caps
cp /path/to/prod.keys prod.keys
node index.js --verbose
```

`npm install` asks for sudo once to run:

```sh
sudo setcap cap_net_admin,cap_net_raw=ep ldn/.venv/bin/python-ldn
```

What happens:

1. `bash/network.sh` disables NetworkManager and brings the radio down;
2. `ldn/main.py` scans for the Civ VII lobby, joins it, and sends the chat
   payload;
3. the same process streams `blob_tpl.bin` + `blob_apply.bin` over UDP 9027;
4. the console answers `I:OK` after the save is committed;
5. the trap restores NetworkManager and the original Wi-Fi connection.

Then close Civ VII on the console, reopen it, and `Load Game`.

Options:

```text
--keys <file>      prod.keys (default ./prod.keys)
--phy <radio>      Wi-Fi radio, e.g. phy0 (auto-detected if only one)
--host <mac>       LDN host MAC when several lobbies are visible
--ifname <name>    temporary interface name (default civ7-ldn)
--blobs <dir>      directory with blob_tpl.bin and blob_apply.bin
--verbose          packet and diagnostic logs
```

Notes:

- This deletes/replaces the `XSTRIG.Civ7Save` trigger save, same as the ESP32
  path.
- If the network is not restored, reconnect Wi-Fi manually before using the
  loader.
- `ldn/` is GPL-3.0-only (CivSwitchLDN + Kinnay's `ldn` library). Keep it in
  its own package.

## Switch 1 NRO (not tested yet)

A second console runs the installer as homebrew, so no PC and no `prod.keys`
are needed.

Requirements:

- a Switch 1 that can run `.nro` homebrew;
- Switch 2 with Civilization VII 1.0.0 (Switch 1 edition cartridge);
- **`ldn_mitm` removed** and the console rebooted — the app hard-blocks while
  a MITM of `ldn:u` is resident;
- both consoles on the same Wi-Fi/LAN for the save-upload stage;
- launch from the **Album** (applet mode). A title override works only when it
  overrides Civilization VII itself.

Build:

```sh
cd packages/transport/nro
export DEVKITPRO=/opt/devkitpro

# Assets are checked in; use them as-is
make

# or regenerate the chat payload, installer and template first
python3 tools/generate_assets.py
make clean all
```

Output: `civ-autoloader-nx.nro`.

Copy it to the SD card and run it from the Album:

```text
/sdcard/switch/civ-autoloader-nx.nro
```

Controls (from the upstream NRO):

| Button | Action |
|---|---|
| Up / Down | select a scanned LDN lobby |
| A | join the lobby |
| X | rescan |
| B | leave the lobby |
| + | exit |

After the lobby reports **JOINED**, install the save. This build writes the
**persistent autoloader save**, so the next `Load Game` runs CivSwitchFS and
kaiHEN:

1. keep the lobby open;
2. trigger the install (the upstream NRO uses **ZR**);
3. wait for `INSTALL DONE`;
4. on the Switch 2: close Civ VII, reopen it, `Load Game`.

**Warning:** like the upstream installer, this replaces every Civilization VII
single-player save. Back up the ones you want to keep.

## Status and known limits

- ESP32: exercised end to end. Fails can be retried by closing and reopening
  the game.
- Linux: shares the CivSwitchLDN protocol layer but has not been run. The
  stage protocol was extended for the 5-field header and `I:OK`/`I:ERR`, which
  the upstream client does not send.
- NRO: builds against devkitPro `libnx`; the payload generation now uses the
  same `build-payload.js` as the other entries. Not run on hardware.
- kaiHEN only arms for Switch 2 games (program ID `0x04...`).
