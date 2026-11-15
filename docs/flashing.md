# Flashing and first run

## Flash the ESP32

```sh
source /path/to/esp-idf/export.sh
cd packages/firmware/esp32
python3 tools/build_and_flash.py --keys prod.keys \
  --port /dev/cu.usbmodemXXXX --flash
```

Add `--erase` for a full chip erase. The board resets after flashing.

## Prepare the SD card

```sh
python3 packages/sdcard/stage.py
cp -R packages/sdcard/contents/* /Volumes/<SD>/
```

The card must contain:

```text
/hbmenu.nro
/kaihen/bootstrap.bin
/kaihen/bin/game/main
/kaihen/bin/hen/main
/kaihen/bin/ro/main
/kaihen/hen-options.txt
```

Use **exFAT** on an updated Switch 2 (microSD Express), or **FAT32** for
smaller cards.

## Install the autoloader save

1. Insert the SD card.
2. Start Civ VII.
3. `MULTIPLAYER -> WIRELESS -> CREATE GAME -> HOST LOBBY`.
4. Power the board. It joins the lobby and sends the autoloader save.
5. Wait for `PERSIST INSTALL DONE - restart the game, then Load Game`.
6. Close Civ VII completely, reopen it, choose **Load Game**, and load
   `XSTRIG.Civ7Save`.

The save runs:

```text
CivSwitchFS -> prepare kaiHEN -> boot kaiHEN -> hbmenu
```

The same-session CFS stage is intentionally not used because it can freeze
the console. If the persistent save causes FS canary failures, the standard
save is a fallback (`make_install_blobs.py` and the `save` stage).

## Manual mode

Set `devMode` to `true` in
`third_party/CivSwitchFS/src/core/autoloader.mjs`, rebuild, and reflash. The
autoloader prepares kaiHEN but does not boot it; the FS bridge stays alive for
manual commands.

## Logs

- ESP32 serial: `python3 tools/wait-log.py --port /dev/cu.usbmodemXXXX`
- HEN SD log: off by default; uncomment `log-sd` in
  `/kaihen/hen-options.txt` to write `/kaihen/hen-log.txt`.
- No SD log is written before kaiHEN starts.
