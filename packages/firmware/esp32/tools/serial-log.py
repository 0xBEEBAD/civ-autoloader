#!/usr/bin/env python3
"""Print the ESP32-S3 USB-Serial/JTAG console without extra dependencies.

  python3 tools/serial-log.py --port /dev/cu.usbmodem2101

Ctrl-C to stop. The LDN firmware also mirrors the console-side loader log
lines received over LDN as "console: ..." lines.
"""
from __future__ import annotations

import argparse
import os
import select
import sys
import termios
import time
import tty


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/cu.usbmodem2101")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    fd = os.open(args.port, os.O_RDONLY | os.O_NONBLOCK)
    tty.setraw(fd)
    attrs = termios.tcgetattr(fd)
    attrs[4] = attrs[5] = args.baud
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    print(f"# {args.port} @ {args.baud}; Ctrl-C to stop", flush=True)
    try:
        while True:
            ready, _, _ = select.select([fd], [], [], 0.25)
            if not ready:
                continue
            try:
                data = os.read(fd, 4096)
            except BlockingIOError:
                continue
            if data:
                sys.stdout.write(data.decode("utf-8", "replace"))
                sys.stdout.flush()
    except KeyboardInterrupt:
        pass
    finally:
        os.close(fd)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
