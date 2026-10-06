"""Run the privileged helper through pkexec and stream its events to the UI (no blocking calls)."""

from __future__ import annotations

import os
import sys
from collections.abc import Callable
from typing import Any

from .events import parse_event

HELPER_PATH = "/usr/lib/caramos-app-center/caramos-app-center-helper"
PKEXEC = "/usr/bin/pkexec"
# pkexec exit codes when the user dismisses or fails the password dialog.
AUTH_CANCELLED = (126, 127)


def helper_command(action: str, ids: list[str]) -> list[str]:
    # Development only: simulate installs without root to work on the UI.
    if os.environ.get("CARAMOS_APP_CENTER_FAKE_HELPER") == "1":
        return [sys.executable, "-m", "caramos_app_center.fake_helper", action, *ids]
    return [PKEXEC, HELPER_PATH, action, *ids]


class HelperRun:
    """One helper invocation. on_event gets each protocol event; on_exit gets (exit code, auth cancelled)."""

    def __init__(self, action: str, ids: list[str], on_event: Callable[[dict[str, Any]], None],
                 on_exit: Callable[[int, bool], None]) -> None:
        from gi.repository import Gio, GLib

        self._Gio = Gio
        self._GLib = GLib
        self.on_event = on_event
        self.on_exit = on_exit
        self.process = Gio.Subprocess.new(helper_command(action, ids),
                                          Gio.SubprocessFlags.STDOUT_PIPE | Gio.SubprocessFlags.STDERR_SILENCE)
        self.reader = Gio.DataInputStream.new(self.process.get_stdout_pipe())
        self._read_next()

    def _read_next(self) -> None:
        self.reader.read_line_async(self._GLib.PRIORITY_DEFAULT, None, self._on_line)

    def _on_line(self, reader, result) -> None:
        try:
            line, _length = reader.read_line_finish_utf8(result)
        except self._GLib.Error:
            line = None
        if line is None:
            self.process.wait_async(None, self._on_wait)
            return
        event = parse_event(line)
        if event is not None:
            self.on_event(event)
        self._read_next()

    def _on_wait(self, process, result) -> None:
        try:
            process.wait_finish(result)
        except self._GLib.Error:
            pass
        code = process.get_exit_status() if process.get_if_exited() else 1
        self.on_exit(code, code in AUTH_CANCELLED)

    def cancel(self) -> None:
        self.process.force_exit()
