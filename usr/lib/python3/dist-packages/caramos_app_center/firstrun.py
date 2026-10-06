"""When the first-run app picker shows up.

Only on systems installed from a CaramOS ISO that enables it (/etc/caramos-app-center/first-run comes
from the ISO overlay), never in the live session (apps installed there are lost at shutdown), and once
per user. Machines that get the App Center through an OTA update only get the menu entry.
"""

from __future__ import annotations

import os
from pathlib import Path

ENABLE_FLAG = Path("/etc/caramos-app-center/first-run")
PROC_CMDLINE = Path("/proc/cmdline")
MARKER_NAME = "first-run-done"


def marker_path(home: Path) -> Path:
    return home / ".config" / "caramos-app-center" / MARKER_NAME


def is_live_session(cmdline: Path = PROC_CMDLINE) -> bool:
    try:
        return "boot=casper" in cmdline.read_text(encoding="utf-8").split()
    except OSError:
        return False


def should_run(
    *,
    home: Path | None = None,
    uid: int | None = None,
    flag: Path = ENABLE_FLAG,
    cmdline: Path = PROC_CMDLINE,
) -> bool:
    home = Path.home() if home is None else home
    uid = os.getuid() if uid is None else uid
    if uid < 1000 or not flag.exists() or is_live_session(cmdline):
        return False
    return not marker_path(home).exists()


def mark_done(home: Path | None = None) -> None:
    marker = marker_path(Path.home() if home is None else home)
    marker.parent.mkdir(parents=True, exist_ok=True)
    marker.touch()
