# Switch 1 NRO transport

Installs Civ VII saves from a **second console** running homebrew, over the
game's local-wireless (LDN) lobby. No PC, no `prod.keys`, no Wi-Fi/LAN.

```text
civ-autoloader NX by sean v0.10
press ZR: install CivSwitch
press R: install autoloader (kaiHEN)
```

**Tested on hardware**: a Switch 1 running this NRO installs the save on a
Switch 2 (23.0.1) over LDN, and `Load Game` reaches kaiHEN.

Based on [xiaolizi23/civswitch-ldn-nx](https://github.com/xiaolizi23/civswitch-ldn-nx)
(GPL-3.0-only).

## What it installs

Both options replace every Civilization VII single-player save with one save
file, `XSTRIG.Civ7Save`. Only the payload inside differs.

| Button | Save | What `Load Game` runs | Next boot |
|---|---|---|---|
| **ZR** | CivSwitch | the standard CivSwitch loader again on TCP `:9026` | keep the loader running, send payloads from the NRO/PC |
| **R** | autoloader (kaiHEN) | CivSwitchFS → prepares and boots **kaiHEN** → hbmenu | insert the SD card and play homebrew |

Use **ZR** when you want the plain CivSwitch loader back after every
`Load Game`. Use **R** when the console should come up into kaiHEN/hbmenu
without any further input. Both writes need the same things; if the FS stage
misbehaves on your firmware, ZR (plain loader) is the fallback.

**Warning:** both options delete all single-player saves of the game. Back them
up first.

## Requirements

- Switch 1 able to run `.nro` homebrew
- Switch 2 with Civilization VII **1.0.0**, Switch 1 edition cartridge
- `ldn_mitm` (or any `ldn:u` MITM) **removed**, console rebooted
- both consoles close to each other (local wireless, no internet needed)
- for the **R** path: the microSD card prepared as described in
  [`../../sdcard/README.md`](../../sdcard/README.md) and inserted **before**
  `Load Game`

## SD card layout for the R path

The autoloader save checks these files; without them the FS stage still
completes but reports the missing files instead of booting kaiHEN.

```text
/hbmenu.nro
/kaihen/bootstrap.bin
/kaihen/bin/ro/main
/kaihen/bin/hen/main
/kaihen/bin/game/main
/licenses/...
```

Generate them once with `python3 packages/sdcard/stage.py` and copy the
**contents** of `packages/sdcard/contents/` to the card root.

## Build

```sh
export DEVKITPRO=/opt/devkitpro
cd packages/transport/nro

# regenerate the three embedded assets from the current payloads
python3 tools/generate_assets.py

make clean all
```

Output: `civ-autoloader-nx.nro`. Copy it to the console:

```text
/sdcard/switch/civ-autoloader-nx.nro
```

`tools/generate_assets.py` produces:

```text
source/chat_payload.h               core.js + the UDP :9027 receiver
source/installer_payload.h          CivSwitchFS persist bootstrap (R)
source/installer_classic_payload.h  standard CivSwitch save (ZR)
data/save_template.bin              pristine save template
```

## Usage on the Switch 1

1. Put the SD card with the NRO into the Switch 1.
2. On the **Switch 2**: start Civilization VII and open
   `MULTIPLAYER → WIRELESS → CREATE GAME → HOST LOBBY`. Leave it open.
3. On the **Switch 1**: launch `civ-autoloader-nx.nro` from the Album
   (applet mode). A hold-R title override is only allowed when it overrides
   Civilization VII itself.
4. Pick one:

   **One-press path (recommended when there is exactly one lobby)**

   - Press **ZR** for CivSwitch, or **R** for the kaiHEN autoloader.
   - The NRO scans immediately. If exactly one Civ VII lobby is found it joins
     it and installs the save without further input.
   - If several lobbies are visible the action stays armed: choose the room
     with Up/Down and press **A**; the install then starts automatically.

   **Manual path**

   - Press **X** to scan.
   - Up/Down to select the lobby, **A** to join.
   - Then **ZR** (CivSwitch) or **R** (autoloader kaiHEN).

5. Wait for the final screen:

   ```text
   Save installer: Persistent trigger save installed
   DONE: restart Civ7 on S2, then Load Game.
   ```

6. On the **Switch 2**: close Civilization VII completely, start it again,
   and choose **Load Game**.

   - **ZR / CivSwitch:** the save brings the CivSwitch loader back on TCP
     `:9026`, ready for the next payload. Leave the game running.
   - **R / autoloader:** the save runs CivSwitchFS, prepares kaiHEN and boots
     it. The console prints `AUTOLOADER: kaiHEN installation complete. You can
     close Civ7 now.` and hbmenu comes up.

## Button map

| Button | Action |
|---|---|
| **ZR** | install CivSwitch save (auto-scan + auto-join when one lobby) |
| **R** | install autoloader (kaiHEN) save (auto-scan + auto-join when one lobby) |
| **X** | rescan |
| **Up / Down** | select a scanned lobby |
| **A** | join the selected lobby (and run an armed install) |
| **Y** | send a test chat line only |
| **B** | leave the lobby |
| **+** | exit |

`R` and `ZR` behave the same whether or not a lobby is joined: with a joined
lobby they install immediately, without one they scan first and join by
themselves when the choice is unambiguous.

## Notes

- Each install needs a **fresh game session**. The chat payload replaces the
  game UI, so the same lobby cannot be reused; restart Civ VII for every
  install.
- The NRO blocks with instructions if `ldn_mitm` is resident, because the LDN
  protocol needs the real `ldn:u` service.
- The transfer stays on the local-wireless link: the NRO binds its LDN address
  on `:9027`, streams base64 chunks, repairs the chunks the radio dropped, and
  waits for the console's `I:OK`.
- A dropped chunk/repair window shows as
  `Timed out repairing installer chunks`; the save was **not** written and the
  run can be repeated after restarting the game.
