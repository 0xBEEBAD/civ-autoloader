#!/usr/bin/env python3
"""Wait for one autoloader run on the ESP32 serial log.

The reader treats a zero-byte read as EOF (USB-Serial-JTAG re-enumeration
after a board reset) and reopens the port, so it keeps streaming across
resets.  Do not grep the whole log for an old BRIDGE line; use
tools/wait_ready.py, which waits for a new line after its baseline.


Start this before entering the Civ VII lobby. It prints the board's USB-Serial
log and exits as soon as a terminal success/failure line appears.

  python3 tools/wait-log.py --port /dev/cu.usbmodem2101

Exit codes: 0 = success keyword, 1 = failure keyword, 2 = timeout/port error.
Only one process should read the serial port at a time; this script warns if
another reader already has it open.
"""
from __future__ import annotations

import argparse
import os
import select
import subprocess
import sys
import termios
import time
import tty

SUCCESS = (
    "AUTOLOADER: kaiHEN installation complete. You can close Civ7 now.",
)
FAILURE = (
    "AUTOLOADER: ERROR:",
    "AUTOLOADER: kaiHEN prepare failed:",
    "AUTOLOADER: HEN: auto failed:",
    "PROMOTION: failed:",
    "UDP installer timed out",
    "LDN authentication failed",
    "all association attempts failed",
    "udp bind failed",
    "no loader found",
)
# A returning payload is terminal even if the relay line was lost.
TERMINAL = ("CIVSWITCHFS RETURNED",)


def readers(port: str) -> list[str]:
    try:
        out = subprocess.run(["lsof", "-t", port], capture_output=True,
                             text=True, timeout=3)
    except Exception:
        return []
    return [line.strip() for line in out.stdout.splitlines() if line.strip()]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/cu.usbmodem2101")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--timeout", type=float, default=900)
    ap.add_argument("--log", default="")
    ap.add_argument("--allow-shared", action="store_true")
    ap.add_argument("--keep", action="store_true",
                    help="do not exit on success/failure (bridge mode)")
    ap.add_argument("--cmd-fifo", default="/tmp/autoloader-cmd",
                    help="FIFO used to type commands into the board serial")
    args = ap.parse_args()

    pids = [pid for pid in readers(args.port) if pid != str(os.getpid())]
    if pids and not args.allow_shared:
        print(f"# serial port {args.port} is already open by PID(s): "
              f"{', '.join(pids)}", file=sys.stderr)
        print("# close the other serial-log.py / monitor first", file=sys.stderr)
        return 2

    log_path = args.log or f"/tmp/autoloader-{int(time.time())}.log"
    log = open(log_path, "w", encoding="utf-8", buffering=1)
    fifo_fd = None
    try:
        if not os.path.exists(args.cmd_fifo):
            os.mkfifo(args.cmd_fifo)
        fifo_fd = os.open(args.cmd_fifo, os.O_RDONLY | os.O_NONBLOCK)
        print(f"# command FIFO: {args.cmd_fifo}", flush=True)
    except OSError as error:
        print(f"# command FIFO unavailable: {error}", flush=True)

    def open_port():
        fd = os.open(args.port, os.O_RDWR | os.O_NONBLOCK)
        tty.setraw(fd)
        attrs = termios.tcgetattr(fd)
        attrs[4] = attrs[5] = args.baud
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        return fd

    print(f"# {args.port} @ {args.baud}; log={log_path}; timeout={args.timeout}s",
          flush=True)

    pending = ""
    deadline = time.time() + args.timeout
    result = 2
    fd = None
    try:
        while time.time() < deadline:
            if fd is None:
                try:
                    fd = open_port()
                    print(f"# opened {args.port}", flush=True)
                except OSError:
                    time.sleep(0.5)
                    continue
            watch = [fd] + ([fifo_fd] if fifo_fd is not None else [])
            try:
                ready, _, _ = select.select(watch, [], [], 0.25)
            except OSError:
                try: os.close(fd)
                except OSError: pass
                fd = None
                continue
            if fifo_fd is not None and fifo_fd in ready:
                try:
                    typed = os.read(fifo_fd, 4096).decode("utf-8", "replace")
                except (BlockingIOError, OSError):
                    typed = ""
                for command in typed.splitlines():
                    if not command:
                        continue
                    try:
                        os.write(fd, (command + "\n").encode())
                        print(f"# sent: {command}", flush=True)
                    except OSError:
                        pass
            if fd not in ready:
                continue
            try:
                data = os.read(fd, 4096)
            except BlockingIOError:
                continue
            except OSError:
                # Board reset / USB-Serial-JTAG re-enumeration.
                try: os.close(fd)
                except OSError: pass
                fd = None
                continue
            if not data:
                # USB-Serial-JTAG re-enumeration: read returns EOF, not an
                # exception. Close and reopen so the next board reset is seen.
                try: os.close(fd)
                except OSError: pass
                fd = None
                print("# serial EOF; reopening", flush=True)
                time.sleep(0.5)
                continue
            text = data.decode("utf-8", "replace")
            log.write(text)
            sys.stdout.write(text)
            sys.stdout.flush()
            pending += text
            while "\n" in pending:
                line, pending = pending.split("\n", 1)
                if args.keep:
                    continue
                for key in FAILURE:
                    if key in line:
                        print(f"\n# FAILURE: {key}", file=sys.stderr)
                        result = 1
                        return result
                for key in TERMINAL:
                    if key in line:
                        print(f"\n# TERMINAL: {key}", file=sys.stderr)
                        result = 0
                        return result
                for key in SUCCESS:
                    if key in line:
                        print(f"\n# SUCCESS: {key}", file=sys.stderr)
                        result = 0
                        return result
        print("\n# TIMEOUT", file=sys.stderr)
        return 2
    except KeyboardInterrupt:
        print("\n# interrupted", file=sys.stderr)
        return result
    finally:
        log.close()
        if fd is not None:
            try: os.close(fd)
            except OSError: pass
        if fifo_fd is not None:
            try: os.close(fifo_fd)
            except OSError: pass


if __name__ == "__main__":
    raise SystemExit(main())
