#!/usr/bin/env python3
"""Wire the autoloader hook into a CivSwitchFS checkout.

The overlay files (autoloader.mjs, autoloader-fs.mjs, autoloader-kaihen.mjs)
live in the checkout's src/core/ directory.  This script only patches
src/core.js so the promotion step calls the autoloader stage.
"""
from __future__ import annotations

import argparse
import os
import sys

IMPORT_FROM = 'import {runPromotionStep} from "./core/promotion.mjs";'
IMPORT_ADD = IMPORT_FROM + '\nimport {run as runAutoloaderStage} from "./core/autoloader.mjs";'
CALL_OLD = 'workerStateSafe: address.addressCopy.workerStateSafe\n    });'
CALL_NEW = ('workerStateSafe: address.addressCopy.workerStateSafe,\n'
            '        onElevated: runAutoloaderStage\n    });')


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", required=True, help="CivSwitchFS checkout")
    args = ap.parse_args()
    root = os.path.abspath(args.dir)
    core = os.path.join(root, "src", "core.js")
    if not os.path.isfile(core):
        print(f"E: {core} is not a CivSwitchFS checkout", file=sys.stderr)
        return 1
    for name in ("autoloader.mjs", "autoloader-fs.mjs", "autoloader-kaihen.mjs"):
        if not os.path.isfile(os.path.join(root, "src", "core", name)):
            print(f"E: missing src/core/{name}; copy the overlay into the checkout first",
                  file=sys.stderr)
            return 4
    text = open(core, encoding="utf-8").read()
    if 'from "./core/autoloader.mjs"' not in text:
        if IMPORT_FROM not in text:
            print("E: promotion import anchor missing", file=sys.stderr)
            return 2
        text = text.replace(IMPORT_FROM, IMPORT_ADD, 1)
    if "onElevated: runAutoloaderStage" not in text:
        if CALL_OLD not in text:
            print("E: promotion call anchor missing", file=sys.stderr)
            return 3
        text = text.replace(CALL_OLD, CALL_NEW, 1)
    open(core, "w", encoding="utf-8").write(text)
    print(f"patched {root}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
