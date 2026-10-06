"""Privileged App Center helper, started through pkexec.

    caramos-app-center-helper install <app-id>...
    caramos-app-center-helper remove <app-id>

It reads the root-owned catalog from its fixed path (never from the environment or arguments),
installs each app with the method the catalog names and writes progress as JSON lines on stdout.
"""

from __future__ import annotations

import fcntl
import os
import re
import subprocess
import sys
import threading
from collections.abc import Callable
from pathlib import Path

from . import flatpak_ops
from .catalog import CATALOG_FILE, ID_RE, RECIPE_DIR, App, Catalog, CatalogError, load_catalog
from .events import emit

LOCK_FILE = Path("/run/caramos-app-center.lock")
MAX_APPS = 40
RECIPE_TIMEOUT = 3600
RECIPE_ENV = {"PATH": "/usr/sbin:/usr/bin:/sbin:/bin", "LANG": "C.UTF-8", "HOME": "/root", "DEBIAN_FRONTEND": "noninteractive"}
APT_BASE = ["apt-get", "-o", "DPkg::Lock::Timeout=600", "-o", "APT::Status-Fd=1", "-y"]
PROGRESS_RE = re.compile(r"^PROGRESS\s+(\d{1,3})\s*$")

EXIT_OK = 0
EXIT_FAILED = 1
EXIT_USAGE = 2


class UsageError(ValueError):
    pass


def parse_args(argv: list[str]) -> tuple[str, list[str]]:
    if len(argv) < 2 or argv[0] not in ("install", "remove"):
        raise UsageError("usage: caramos-app-center-helper install|remove <app-id>...")
    action, ids = argv[0], argv[1:]
    if len(ids) > MAX_APPS or (action == "remove" and len(ids) != 1):
        raise UsageError("too many app ids")
    for app_id in ids:
        if not ID_RE.fullmatch(app_id):
            raise UsageError(f"invalid app id: {app_id!r}")
    if len(set(ids)) != len(ids):
        raise UsageError("duplicate app ids")
    return action, ids


def apt_progress(line: str) -> int | None:
    """Map APT::Status-Fd lines (dlstatus:/pmstatus:) to 0-100, downloads first."""

    parts = line.split(":", 3)
    if len(parts) < 3 or parts[0] not in ("dlstatus", "pmstatus"):
        return None
    try:
        percent = float(parts[2])
    except ValueError:
        return None
    return int(percent / 2) if parts[0] == "dlstatus" else int(50 + percent / 2)


def _stream(args: list[str], on_line: Callable[[str], None], *, env: dict[str, str] | None = None,
            timeout: int = RECIPE_TIMEOUT) -> int:
    process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=env,
                               stdin=subprocess.DEVNULL)
    timer = threading.Timer(timeout, process.kill)
    timer.start()
    try:
        assert process.stdout is not None
        for raw in process.stdout:
            on_line(raw.rstrip("\n"))
        return process.wait()
    finally:
        timer.cancel()


class Helper:
    def __init__(self, catalog: Catalog, recipe_dir: Path = RECIPE_DIR, stream=None) -> None:
        self.catalog = catalog
        self.recipe_dir = recipe_dir
        self.stream = stream
        self.apt_updated = False

    def emit(self, event: str, **fields) -> None:
        emit(event, self.stream, **fields)

    def _progress(self, app: App, percent: int, message: str = "") -> None:
        self.emit("progress", id=app.id, percent=max(0, min(100, percent)), message=message)

    def _log(self, app: App, line: str) -> None:
        if line.strip():
            self.emit("log", id=app.id, line=line)

    def _recipe(self, app: App, action: str) -> None:
        recipe = self.recipe_dir / app.recipe
        if not (recipe.is_file() and os.access(recipe, os.X_OK)):
            raise RuntimeError(f"recipe {app.recipe} is missing")

        def on_line(line: str) -> None:
            match = PROGRESS_RE.match(line)
            if match:
                self._progress(app, int(match.group(1)))
            else:
                self._log(app, line)

        code = _stream([str(recipe), action], on_line, env=dict(RECIPE_ENV))
        if code != 0:
            raise RuntimeError(f"{app.recipe} {action} exited with {code}")

    def _apt(self, app: App, action: str) -> None:
        env = dict(RECIPE_ENV)

        def on_line(line: str) -> None:
            percent = apt_progress(line)
            if percent is not None:
                self._progress(app, percent)
            elif not line.startswith(("dlstatus:", "pmstatus:", "media-change:")):
                self._log(app, line)

        if action == "install" and not self.apt_updated:
            self._log(app, "apt-get update")
            if _stream(["apt-get", "-o", "DPkg::Lock::Timeout=600", "update"], lambda line: self._log(app, line), env=env) != 0:
                raise RuntimeError("apt-get update failed")
            self.apt_updated = True
        verb = "install" if action == "install" else "remove"
        if _stream([*APT_BASE, verb, "--", *app.apt_packages], on_line, env=env) != 0:
            raise RuntimeError(f"apt-get {verb} failed")
        self._progress(app, 100)

    def _flatpak(self, app: App, action: str) -> None:
        operation = flatpak_ops.install if action == "install" else flatpak_ops.uninstall
        operation(app.flatpak_id, lambda percent, message: self._progress(app, percent, message), lambda line: self._log(app, line))

    def run_one(self, app: App, action: str) -> bool:
        try:
            if app.preinstalled:
                raise RuntimeError(f"{app.name} comes with CaramOS")
            {"recipe": self._recipe, "apt": self._apt, "flatpak": self._flatpak}[app.method](app, action)
        except Exception as exc:  # report and carry on with the next app
            self.emit("done", id=app.id, ok=False, error=str(exc))
            return False
        self.emit("done", id=app.id, ok=True)
        return True

    def run(self, action: str, ids: list[str]) -> int:
        failed = []
        for index, app_id in enumerate(ids, 1):
            app = self.catalog.apps.get(app_id)
            if app is None:
                self.emit("done", id=app_id, ok=False, error="unknown app")
                failed.append(app_id)
                continue
            self.emit("start", id=app_id, index=index, total=len(ids), action=action)
            if not self.run_one(app, action):
                failed.append(app_id)
        self.emit("finished", ok=not failed, failed=failed)
        return EXIT_FAILED if failed else EXIT_OK


def main(argv: list[str] | None = None) -> int:
    argv = sys.argv[1:] if argv is None else argv
    try:
        action, ids = parse_args(argv)
    except UsageError as exc:
        print(exc, file=sys.stderr)
        return EXIT_USAGE
    if os.geteuid() != 0:
        print("caramos-app-center-helper must run as root (through pkexec)", file=sys.stderr)
        return EXIT_USAGE
    try:
        catalog = load_catalog(CATALOG_FILE, RECIPE_DIR)
    except CatalogError as exc:
        print(exc, file=sys.stderr)
        return EXIT_FAILED
    with LOCK_FILE.open("w") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            print("another App Center installation is running", file=sys.stderr)
            return EXIT_FAILED
        return Helper(catalog).run(action, ids)
