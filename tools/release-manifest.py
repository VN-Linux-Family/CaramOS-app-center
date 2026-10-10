#!/usr/bin/env python3
"""Print latest.json for a built .deb: the update manifest CaramOS machines verify and install.

Used by .github/workflows/release.yml, which signs the output (latest.json.sig) with the App Center
release key. caramos-ota (caramos_ota/app_center.py) checks that signature, the file name and the
SHA-256 before installing the .deb.
"""

from __future__ import annotations

import hashlib
import json
import re
import sys
from pathlib import Path

PACKAGE = "caramos-app-center"


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print(f"usage: {argv[0]} dist/{PACKAGE}_<version>_all.deb", file=sys.stderr)
        return 2
    deb = Path(argv[1])
    match = re.fullmatch(rf"{PACKAGE}_([0-9][0-9A-Za-z.+~]*)_all\.deb", deb.name)
    if match is None:
        print(f"unexpected file name {deb.name}", file=sys.stderr)
        return 1
    manifest = {
        "package": PACKAGE,
        "version": match.group(1),
        "file": deb.name,
        "sha256": hashlib.sha256(deb.read_bytes()).hexdigest(),
    }
    print(json.dumps(manifest, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
