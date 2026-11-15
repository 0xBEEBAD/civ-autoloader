#!/usr/bin/env python3
"""Build blob_tpl.bin and blob_apply.bin for the persistent autoloader save.

The CivSwitchFS persist build produces:

  dist/persist/save-template.bin     pristine Civ7 save template
  dist/persist/persist-bootstrap.js  save installer + compressed CFS stage

The ESP32 streams the template as T<i> chunks and the bootstrap as J<i>
chunks. The console writes the save; Load Game then runs the autoloader.
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
MAX_TEMPLATE = 2_000_000
MAX_BOOTSTRAP = 500_000


def write_blob(path: str, data: bytes) -> int:
    encoded = base64.b64encode(data)
    with open(path, "wb") as fh:
        fh.write(struct.pack("<I", len(encoded)))
        fh.write(encoded)
    return len(encoded)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cfs", default=CFS, help="CivSwitchFS checkout")
    ap.add_argument("--out", default=BLOBS)
    ap.add_argument("--no-build", action="store_true",
                    help="use an existing dist/persist build")
    args = ap.parse_args()

    root = os.path.abspath(args.cfs)
    if not args.no_build:
        subprocess.run(["npm", "run", "persist:build"], cwd=root, check=True)

    template = open(os.path.join(root, "dist", "persist",
                                 "save-template.bin"), "rb").read()
    bootstrap = open(os.path.join(root, "dist", "persist",
                                  "persist-bootstrap.js"), "rb").read()
    if not template or len(template) > MAX_TEMPLATE:
        print(f"E: template is {len(template)} bytes", file=sys.stderr)
        return 1
    if not bootstrap or len(bootstrap) > MAX_BOOTSTRAP:
        print(f"E: bootstrap is {len(bootstrap)} bytes", file=sys.stderr)
        return 1

    os.makedirs(args.out, exist_ok=True)
    tpl_len = write_blob(os.path.join(args.out, "blob_tpl.bin"), template)
    apply_len = write_blob(os.path.join(args.out, "blob_apply.bin"), bootstrap)
    print(f"template {len(template)} -> base64 {tpl_len}")
    print(f"bootstrap {len(bootstrap)} -> base64 {apply_len}")
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
