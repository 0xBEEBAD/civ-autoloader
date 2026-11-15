#!/usr/bin/env python3
"""Send CFS bridge commands and show the board log for every step.

The ESP32 serial log is append-only across board resets.  Every command here
records the current line count first and only prints lines that arrive after
that baseline, so old sessions never trigger a false success.

  # one command (prints new log lines, then OK/TIMEOUT)
  python3 tools/console.py send "dev on"
  python3 tools/console.py send "launchloop" --timeout 180

  # several commands; CMD:seconds overrides the per-command timeout
  python3 tools/console.py run \
      "dev on" "pmdmnt 2 3700000000000004" "launchloop:180" \
      "pmstart 209" "smget ro:dmnt" "ldrro" "henboot:240"

  # wait for a new line without sending anything
  python3 tools/console.py wait "bridge: console ready" --timeout 300

  # last lines of the log
  python3 tools/console.py tail 30

A step never hangs silently: on timeout it prints "TIMEOUT" and the last log
lines, and exits non-zero.  Use --keep-going with `run` to continue anyway.

Exit codes: 0 all steps got a reply/match, 2 timeout, 3 missing FIFO/log.
"""
from __future__ import annotations

import argparse
import os
import re
import sys
import time

DEFAULT_LOG = "/tmp/autoloader-live.log"
DEFAULT_FIFO = "/tmp/autoloader-cmd"
REPLY = "bridge: reply"

# Typical durations of the kaiHEN flow; only used when a step has no :seconds.
TIMEOUTS = {
    "dev": 10,
    "ping": 10,
    "pmdmnt": 25,
    "pmstart": 25,
    "pmshell": 25,
    "smget": 30,
    "smstate": 25,
    "ldrro": 40,
    "henboot": 300,
    "launchloop": 180,
    "rostate": 25,
    "hencheck": 30,
}


def read_lines(path: str) -> list[str]:
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            return fh.read().splitlines()
    except FileNotFoundError:
        return []


def tail(path: str, count: int) -> None:
    for line in read_lines(path)[-count:]:
        print("   | " + line[:260], flush=True)


def split_step(step: str) -> tuple[str, float | None]:
    match = re.match(r"^(.*):([0-9]+(?:\.[0-9]+)?)$", step)
    if not match:
        return step, None
    return match.group(1), float(match.group(2))


def default_timeout(command: str) -> float:
    verb = command.strip().split(" ", 1)[0]
    return float(TIMEOUTS.get(verb, 25))


def wait_for(
    log: str,
    fifo: str,
    command: str | None,
    match: str,
    timeout: float,
    quiet: bool = False,
) -> bool:
    if command is not None:
        if not os.path.exists(fifo):
            print("!! missing FIFO " + fifo, flush=True)
            return False
        baseline = len(read_lines(log))
        print(">> " + command, flush=True)
        with open(fifo, "w") as fh:
            fh.write(command + "\n")
    else:
        baseline = len(read_lines(log))
        print(".. waiting for /" + match + "/", flush=True)

    pattern = re.compile(match)
    deadline = time.time() + timeout
    printed = baseline
    while time.time() < deadline:
        lines = read_lines(log)
        if len(lines) < baseline:
            baseline = 0  # log rotated/truncated
        for line in lines[printed:]:
            if not quiet:
                print("   | " + line[:260], flush=True)
        printed = max(printed, len(lines))
        for line in lines[baseline:]:
            if pattern.search(line):
                print("<= " + (command or match) + "  [" + f"{timeout - (deadline - time.time()):.1f}s]",
                      flush=True)
                return True
        time.sleep(0.2)

    print("!! TIMEOUT after " + f"{timeout:.0f}s: " + (command or match), flush=True)
    tail(log, 8)
    return False


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--log", default=DEFAULT_LOG)
    ap.add_argument("--fifo", default=DEFAULT_FIFO)
    ap.add_argument("--keep-going", action="store_true")
    sub = ap.add_subparsers(dest="action", required=True)

    send = sub.add_parser("send")
    send.add_argument("command")
    send.add_argument("--timeout", type=float)
    send.add_argument("--match", default=REPLY)

    run = sub.add_parser("run")
    run.add_argument("steps", nargs="+")
    run.add_argument("--match", default=REPLY)

    wait = sub.add_parser("wait")
    wait.add_argument("match")
    wait.add_argument("--timeout", type=float, default=120)
    wait.add_argument("--quiet", action="store_true")

    show = sub.add_parser("tail")
    show.add_argument("count", nargs="?", type=int, default=30)

    args = ap.parse_args()

    if args.action == "tail":
        tail(args.log, args.count)
        return 0

    if not os.path.exists(args.log):
        print("!! missing log " + args.log, flush=True)
        return 3

    if args.action == "wait":
        ok = wait_for(args.log, args.fifo, None, args.match, args.timeout,
                      args.quiet)
        return 0 if ok else 2

    if args.action == "send":
        timeout = args.timeout if args.timeout is not None else default_timeout(args.command)
        ok = wait_for(args.log, args.fifo, args.command, args.match, timeout)
        return 0 if ok else 2

    # run
    all_ok = True
    for raw in args.steps:
        command, seconds = split_step(raw)
        timeout = seconds if seconds is not None else default_timeout(command)
        ok = wait_for(args.log, args.fifo, command, args.match, timeout)
        all_ok = all_ok and ok
        if not ok and not args.keep_going:
            print("-- stopping after failed step", flush=True)
            return 2
    return 0 if all_ok else 2


if __name__ == "__main__":
    sys.exit(main())
