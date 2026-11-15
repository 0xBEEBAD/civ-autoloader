#!/usr/bin/env python3
"""Stage the SD card contents from vendor/ and the pinned prebuilt files."""
from __future__ import annotations

import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
VENDOR = os.path.join(HERE, "vendor")
CONTENTS = os.path.join(HERE, "contents")


def copy(src: str, dst: str) -> None:
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copy2(src, dst)


def optional(src: str, dst: str) -> None:
    if os.path.exists(src):
        copy(src, dst)


def main() -> int:
    if os.path.isdir(CONTENTS):
        shutil.rmtree(CONTENTS)
    os.makedirs(CONTENTS)

    copy(os.path.join(VENDOR, "hbmenu.nro"),
         os.path.join(CONTENTS, "hbmenu.nro"))
    copy(os.path.join(VENDOR, "game", "main"),
         os.path.join(CONTENTS, "kaihen", "bin", "game", "main"))
    copy(os.path.join(VENDOR, "hen", "main"),
         os.path.join(CONTENTS, "kaihen", "bin", "hen", "main"))
    copy(os.path.join(VENDOR, "ro", "main"),
         os.path.join(CONTENTS, "kaihen", "bin", "ro", "main"))
    copy(os.path.join(VENDOR, "bootstrap.bin"),
         os.path.join(CONTENTS, "kaihen", "bootstrap.bin"))
    copy(os.path.join(VENDOR, "hen-options.txt"),
         os.path.join(CONTENTS, "kaihen", "hen-options.txt"))
    optional(os.path.join(VENDOR, "plugins", "ftpd.nso"),
             os.path.join(CONTENTS, "kaihen", "plugins", "ftpd.nso"))
    shutil.copytree(os.path.join(HERE, "licenses"),
                    os.path.join(CONTENTS, "licenses"))

    print(f"staged {CONTENTS}")
    print("copy contents/* to the root of the SD card")
    return 0


if __name__ == "__main__":
    sys.exit(main())
