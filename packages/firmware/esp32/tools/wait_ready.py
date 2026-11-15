#!/usr/bin/env python3
"""Wait for a *new* bridge-ready line in the autoloader serial log.

The ESP32 log is append-only across board resets.  Grepping the whole file
therefore matches old sessions and makes a monitor hang or fire early.  This
script records the current line count, then waits only for lines after that
baseline.

  python3 tools/wait_ready.py                 # 300s default
  python3 tools/wait_ready.py --timeout 600
  python3 tools/wait_ready.py --log /tmp/autoloader-live.log

Exit: 0 ready, 2 timeout.
"""
from __future__ import annotations

import argparse
import os
import sys
import time

READY_MARKERS = (
    "bridge: console ready",
    "AUTOLOADER: BRIDGE: ready",
)


def read_lines(path: str) -> list[str]:
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            return fh.read().splitlines()
    except FileNotFoundError:
        return []


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--log", default="/tmp/autoloader-live.log")
    ap.add_argument("--timeout", type=float, default=300)
    ap.add_argument("--quiet", action="store_true",
                    help="only print ready/timeout markers")
    args = ap.parse_args()

    # Baseline is the current line count.  If the file is truncated/rotated,
    # reset the baseline so a fresh boot is still seen.
    baseline = len(read_lines(args.log))
    deadline = time.time() + args.timeout
    print(f"# waiting for a NEW bridge line in {args.log} "
          f"(baseline={baseline}, timeout={args.timeout}s)", flush=True)

    while time.time() < deadline:
        lines = read_lines(args.log)
        if len(lines) < baseline:
            baseline = 0
        new = lines[baseline:]
        baseline = len(lines)
        for line in new:
            if not args.quiet:
                print(line, flush=True)
            if any(marker in line for marker in READY_MARKERS):
                print("# READY", flush=True)
                return 0
            if "bridge: reply" in line and "pong" in line:
                print("# READY (pong)", flush=True)
                return 0
        time.sleep(0.25)

    print("# TIMEOUT: no new bridge line", flush=True)
    for line in read_lines(args.log)[-10:]:
        print(line, flush=True)
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
