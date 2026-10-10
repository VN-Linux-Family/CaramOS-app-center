"""When the first-run app picker shows up.

On systems started or installed from a CaramOS ISO that enables it (/etc/caramos-app-center/first-run
comes from the ISO overlay), once per user: in the live session from the USB (its home is in RAM, so
every boot of the USB shows it again) and at the first login after installing. Machines that get the
App Center through an OTA update only get the menu entry and the Desktop launcher.
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
    # The CaramOS live user is uid 1000, but casper's default live user is uid 999; installed systems
    # start desktop users at 1000.
    first_user_uid = 999 if is_live_session(cmdline) else 1000
    if uid < first_user_uid or not flag.exists():
        return False
    return not marker_path(home).exists()


def mark_done(home: Path | None = None) -> None:
    marker = marker_path(Path.home() if home is None else home)
    marker.parent.mkdir(parents=True, exist_ok=True)
    marker.touch()
