#!/usr/bin/env python3
"""One-click build (and optional flash) for the civ-autoloader ESP32 firmware.

  python3 tools/build_and_flash.py --keys prod.keys --port /dev/cu.usbmodemXXXX --flash

Steps:
  1. main/keys.h        from prod.keys        (make_keys.py)
  2. main/chat_payload.h from third_party/CivSwitch/core.js (make_payload.py)
  3. blobs/*.bin        from installer-creator (make_install_blobs.py)
  4. idf.py build
  5. esptool write-flash (bootloader, partition table, app, template, apply JS)

Requires the ESP-IDF environment to be sourced (idf.py on PATH) and node.
"""
from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from datetime import datetime, timezone

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)                       # packages/firmware/esp32
PROJECT = os.path.dirname(os.path.dirname(os.path.dirname(ROOT)))
CIV = os.path.join(PROJECT, "third_party", "CivSwitch")
CFS = os.path.join(PROJECT, "third_party", "CivSwitchFS")
BUILD = os.path.join(ROOT, "build")
BLOBS = os.path.join(ROOT, "blobs")

APP_OFF = 0x10000
TPL_OFF = 0x110000
APPLY_OFF = 0x290000


def run(cmd, **kw):
    print("+", " ".join(cmd), flush=True)
    return subprocess.run(cmd, check=True, **kw)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--keys", default=os.path.join(ROOT, "prod.keys"),
                    help="path to prod.keys (required unless main/keys.h exists)")
    ap.add_argument("--core", default=os.path.join(CIV, "core.js"),
                    help="shared CivSwitch core.js")
    ap.add_argument("--cfs", default=CFS,
                    help="CivSwitchFS checkout; builds blobs/blob_cfs.bin")
    ap.add_argument("--port", default=os.environ.get("ESPPORT", ""),
                    help="serial port for flashing, e.g. /dev/cu.usbmodemXXXX")
    ap.add_argument("--flash", action="store_true", help="flash after building")
    ap.add_argument("--erase", action="store_true", help="erase the whole flash first")
    ap.add_argument("--no-build", action="store_true", help="skip idf.py build")
    args = ap.parse_args()

    py = sys.executable
    stamp = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    print(f"build time (UTC): {stamp}", flush=True)
    build_h = os.path.join(ROOT, "main", "build_time.h")
    with open(build_h, "w", encoding="utf-8") as fh:
        fh.write('#pragma once\n#define BUILD_UTC "' + stamp + '"\n')
    overlay_src = os.path.join(args.cfs, "src", "core", "autoloader.mjs")
    overlay_original = None
    if os.path.exists(overlay_src):
        overlay_original = open(overlay_src, encoding="utf-8").read()
        text = re.sub(r'const BUILD_UTC = "[^"]*";',
                      'const BUILD_UTC = "' + stamp + '";', overlay_original, count=1)
        open(overlay_src, "w", encoding="utf-8").write(text)
        print(f"overlay build time: {stamp}", flush=True)

    keys_h = os.path.join(ROOT, "main", "keys.h")
    if os.path.exists(args.keys):
        run([py, os.path.join(HERE, "make_keys.py"), args.keys])
    elif os.path.exists(keys_h):
        print("using existing main/keys.h")
    else:
        print("E: no prod.keys found (pass --keys)", file=sys.stderr)
        return 1

    run([py, os.path.join(HERE, "make_payload.py"), args.core])
    try:
        # Default install: the persistent autoloader save. The save runs CFS
        # on the next Load Game; the same-session CFS stage is not used.
        run([py, os.path.join(HERE, "make_persist_blobs.py"),
             "--cfs", args.cfs])
    finally:
        # BUILD_UTC is a build-time value.  Do not leave the stamped overlay
        # in the working tree; the next build stamps it again.
        if overlay_original is not None:
            open(overlay_src, "w", encoding="utf-8").write(overlay_original)

    if not args.no_build:
        run(["idf.py", "build"], cwd=ROOT)

    if not args.flash:
        print("build only; pass --flash --port PORT to flash")
        return 0

    if not args.port:
        print("E: --port is required with --flash", file=sys.stderr)
        return 1

    esptool = [py, "-m", "esptool", "--chip", "esp32s3", "-p", args.port, "-b", "460800",
               "--before", "default-reset", "--after", "hard-reset"]
    if args.erase:
        run(esptool + ["erase-flash"])
    run(esptool + ["write-flash", "--flash-mode", "dio", "--flash-freq", "80m",
                   "--flash-size", "16MB",
                   "0x0", os.path.join(BUILD, "bootloader", "bootloader.bin"),
                   "0x8000", os.path.join(BUILD, "partition_table", "partition-table.bin"),
                   hex(APP_OFF), os.path.join(BUILD, "esp32-ldn-civ7.bin"),
                   hex(TPL_OFF), os.path.join(BLOBS, "blob_tpl.bin"),
                   hex(APPLY_OFF), os.path.join(BLOBS, "blob_apply.bin")])
    print("flashed. Press RESET on the board, then create the wireless lobby.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
