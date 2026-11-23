"""After a failed install or removal: ask VNLF for help with what the helper logged.

The install view keeps each app's helper output while it runs. "Ask for help" saves the output of the apps
that failed to ~/.cache/caramos-app-center/install-failure.log and opens Feedback & Help (caramos-help, part
of caramos-ota) with that file, which takes personal data out of it and attaches it for VNLF's team only.
"""

from __future__ import annotations

import functools
import os
import subprocess
from collections.abc import Mapping, Sequence
from datetime import datetime
from pathlib import Path

from .catalog import App
from .i18n import _

HELP_COMMAND = "/usr/bin/caramos-help"
MAX_LINES = 400   # the end of each app's output


@functools.cache
def help_available() -> bool:
    """caramos-help is there and takes --log: caramos-ota older than the App Center's button has no such option."""

    if not os.access(HELP_COMMAND, os.X_OK):
        return False
    try:
        result = subprocess.run([HELP_COMMAND, "--help"], check=False, capture_output=True, text=True, timeout=10)
    except (OSError, subprocess.SubprocessError):
        return False
    return "--log" in result.stdout


def failure_log_path() -> Path:
    cache = os.environ.get("XDG_CACHE_HOME") or os.path.join(os.path.expanduser("~"), ".cache")
    return Path(cache) / "caramos-app-center" / "install-failure.log"


def _version() -> str:
    try:
        result = subprocess.run(["dpkg-query", "-W", "-f=${Version}", "caramos-app-center"], check=False,
                                capture_output=True, text=True, timeout=10)
    except (OSError, subprocess.SubprocessError):
        return "?"
    return result.stdout.strip() or "?"


def failed(apps: Sequence[App], results: Mapping[str, tuple[bool, str]]) -> list[App]:
    return [app for app in apps if not results.get(app.id, (False, ""))[0]]


def failure_log_text(action: str, apps: Sequence[App], results: Mapping[str, tuple[bool, str]],
                     logs: Mapping[str, Sequence[str]], version: str | None = None,
                     when: datetime | None = None) -> str:
    moment = (when or datetime.now()).astimezone().isoformat(timespec="seconds")
    lines = [f"App Center {version or _version()}: {action} failed at {moment}"]
    for app in failed(apps, results):
        error = results.get(app.id, (False, ""))[1]
        lines += ["", f"== {app.name} ({app.id}, {app.method}: {', '.join(app.targets)}) ==",
                  f"Reason: {error or 'not started'}"]
        output = list(logs.get(app.id, ()))[-MAX_LINES:]
        lines += output or ["(no output)"]
    return "\n".join(lines) + "\n"


def ask_for_help(action: str, apps: Sequence[App], results: Mapping[str, tuple[bool, str]],
                 logs: Mapping[str, Sequence[str]], message: str, when: datetime | None = None) -> bool:
    """Save the failed apps' output and open Feedback & Help with it and message; False when that did not work.
    when: the time the apps failed."""

    if not failed(apps, results):
        return False
    path = failure_log_path()
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(failure_log_text(action, apps, results, logs, when=when), encoding="utf-8")
        subprocess.Popen([HELP_COMMAND, "--type", "bug", "--message", message, "--log", str(path)],
                         stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                         start_new_session=True)
    except OSError:
        return False
    return True


def failure_text(event: dict) -> str:
    """Why an app was not installed: in the user's language when the helper knows, else what it reported."""

    code = event.get("code")
    packages = ", ".join(str(name) for name in event.get("packages") or [])
    if code == "broken-packages" and packages:
        text = _("%s did not finish installing, so every other install fails too.") % packages
        if "nvidia" in packages:
            text += " " + _("Open Driver Check to replace the NVIDIA driver, then try again.")
        return text
    if code == "busy":
        return _("Another program is installing software (the CaramOS installer or the Update Manager). "
                 "Wait until it is done, then try again.")
    return str(event.get("error") or "")
