#!/usr/bin/env python3
"""Prepend a debian/changelog entry for a new version; the release refuses to run until it is edited."""

from __future__ import annotations

import re
import sys
from datetime import datetime
from email.utils import format_datetime
from pathlib import Path

CHANGELOG = Path(__file__).resolve().parents[1] / "debian/changelog"
VERSION_RE = re.compile(r"^\d+\.\d+\.\d+$")


def main(argv: list[str]) -> int:
    if len(argv) != 2 or not VERSION_RE.fullmatch(argv[1]):
        print("usage: new-changelog-entry.py X.Y.Z", file=sys.stderr)
        return 2
    version = argv[1]
    current = CHANGELOG.read_text(encoding="utf-8")
    if current.startswith(f"caramos-app-center ({version})"):
        print(f"debian/changelog already starts at {version}")
        return 0
    entry = (
        f"caramos-app-center ({version}) noble; urgency=medium\n\n"
        "  * Describe the changes of this release.\n\n"
        " -- Vietnam Linux Family <developer@vietnamlinuxfamily.net>  "
        f"{format_datetime(datetime.now().astimezone())}\n\n"
    )
    CHANGELOG.write_text(entry + current, encoding="utf-8")
    print(f"Added {version} to debian/changelog; replace the placeholder line with the changes.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
