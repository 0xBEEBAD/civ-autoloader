#!/usr/bin/env python3
"""Build blobs/blob_cfs.bin from a CivSwitchFS checkout.

The blob is:
    u32 LE length + base64(dist/core.js)

It is streamed over the existing LDN UDP installer channel after the original
CivSwitch save is installed.  The console never writes the CivSwitchFS payload
to the save; it only evaluates it in the running loader.
"""
from __future__ import annotations

import argparse
import base64
import os
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
PROJECT = os.path.dirname(os.path.dirname(os.path.dirname(ROOT)))
CFS = os.path.join(PROJECT, "third_party", "CivSwitchFS")
BLOBS = os.path.join(ROOT, "blobs")
OUT = os.path.join(BLOBS, "blob_cfs.bin")
MAX_LOADER_BYTES = 500_000


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", default=CFS,
                    help="CivSwitchFS checkout (contains package.json)")
    ap.add_argument("--no-build", action="store_true",
                    help="use an existing dist/core.js")
    ap.add_argument("--out", default=OUT)
    args = ap.parse_args()

    root = os.path.abspath(args.dir)
    core = os.path.join(root, "dist", "core.js")
    if not args.no_build:
        if not os.path.isfile(os.path.join(root, "package.json")):
            print(f"E: {root} is not a CivSwitchFS checkout", file=sys.stderr)
            return 1
        # build_and_flash.py stamps BUILD_UTC into the overlay and restores
        # it after the build.  Keep a copy as a safety net.
        autoloader = os.path.join(root, "src", "core", "autoloader.mjs")
        original = None
        if os.path.isfile(autoloader):
            original = open(autoloader, encoding="utf-8").read()
        try:
            subprocess.run([sys.executable,
                            os.path.join(HERE, "prepare_cfs.py"),
                            "--dir", root], check=True)
            subprocess.run(["npm", "run", "build"], cwd=root, check=True)
        finally:
            if original is not None:
                open(autoloader, "w", encoding="utf-8").write(original)
    if not os.path.isfile(core):
        print(f"E: missing {core}; run without --no-build", file=sys.stderr)
        return 1

    payload = open(core, "rb").read()
    if not payload or len(payload) >= MAX_LOADER_BYTES:
        print(f"E: payload is {len(payload)} bytes (limit {MAX_LOADER_BYTES})",
              file=sys.stderr)
        return 1
    encoded = base64.b64encode(payload)
    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, "wb") as f:
        f.write(struct.pack("<I", len(encoded)))
        f.write(encoded)
    print(f"core {len(payload)} bytes -> base64 {len(encoded)} bytes")
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
