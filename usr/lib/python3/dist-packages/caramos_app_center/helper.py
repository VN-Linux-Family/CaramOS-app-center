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
import time
from collections.abc import Callable
from pathlib import Path

from . import detect, flatpak_ops
from .catalog import CATALOG_FILE, ID_RE, RECIPE_DIR, App, Catalog, CatalogError, load_catalog
from .events import emit

LOCK_FILE = Path("/run/caramos-app-center.lock")
MAX_APPS = 40
RECIPE_TIMEOUT = 3600
RECIPE_ENV = {"PATH": "/usr/sbin:/usr/bin:/sbin:/bin", "LANG": "C.UTF-8", "HOME": "/root", "DEBIAN_FRONTEND": "noninteractive"}
APT_BASE = ["apt-get", "-o", "DPkg::Lock::Timeout=600", "-o", "APT::Status-Fd=1", "-y"]
PROGRESS_RE = re.compile(r"^PROGRESS\s+(\d{1,3})\s*$")
REASON_MAX = 300
# debconf's database, which a running installer keeps locked (ubiquity on the live USB, Update Manager): a
# package that asks a debconf question cannot be installed meanwhile ("config.dat is locked", ticket H-1091).
DEBCONF_DB = Path("/var/cache/debconf/config.dat")
DEBCONF_LOCKED = "config.dat is locked by another process"
DEBCONF_WAIT = 900
DEBCONF_POLL = 3
# The line dpkg prints before the packages it could not configure.
DPKG_ERRORS = "Errors were encountered while processing:"

EXIT_OK = 0
EXIT_FAILED = 1
EXIT_USAGE = 2


class UsageError(ValueError):
    pass


class InstallError(RuntimeError):
    """A failure the App Center explains in the user's language (code), with the packages it is about."""

    def __init__(self, message: str, code: str = "", packages: tuple[str, ...] = ()) -> None:
        super().__init__(message)
        self.code = code
        self.packages = packages


def debconf_locked(path: Path = DEBCONF_DB) -> bool:
    try:
        with path.open("rb") as handle:
            try:
                fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                return True
            fcntl.flock(handle, fcntl.LOCK_UN)
    except OSError:
        return False
    return False


class DpkgErrors:
    """Reads the packages dpkg could not configure, and whether debconf's database was locked, out of apt-get or
    recipe output."""

    def __init__(self) -> None:
        self.packages: list[str] = []
        self.debconf_locked = False
        self._listing = False

    def feed(self, line: str) -> None:
        if DEBCONF_LOCKED in line:
            self.debconf_locked = True
        if line.strip() == DPKG_ERRORS:
            self._listing = True
        elif self._listing and line.startswith(" ") and line.strip():
            name = line.strip()
            if name not in self.packages:
                self.packages.append(name)
        else:
            self._listing = False


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
        self.dpkg_errors = DpkgErrors()
        self.debconf_locked = debconf_locked
        self.debconf_wait = DEBCONF_WAIT
        self.installed_apps = detect.installed_apps

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

        last_line = ""

        def on_line(line: str) -> None:
            nonlocal last_line
            match = PROGRESS_RE.match(line)
            if match:
                self._progress(app, int(match.group(1)))
            else:
                self._log(app, line)
                self.dpkg_errors.feed(line)
                last_line = line.strip() or last_line

        code = _stream([str(recipe), action], on_line, env=dict(RECIPE_ENV))
        if code != 0:
            # What a recipe printed last says why it stopped (its own message, curl or apt-get).
            raise RuntimeError(last_line[:REASON_MAX] or f"{app.recipe} {action} exited with {code}")

    def _apt(self, app: App, action: str) -> None:
        env = dict(RECIPE_ENV)

        def on_line(line: str) -> None:
            percent = apt_progress(line)
            if percent is not None:
                self._progress(app, percent)
            elif not line.startswith(("dlstatus:", "pmstatus:", "media-change:")):
                self._log(app, line)
                self.dpkg_errors.feed(line)

        if action == "install" and not self.apt_updated:
            self._log(app, "apt-get update")
            # A source that no longer works (another program's repository, the CD-ROM line the installer adds
            # on the live USB) fails apt-get update as a whole, though the other lists are fresh.
            if _stream(["apt-get", "-o", "DPkg::Lock::Timeout=600", "update"], lambda line: self._log(app, line), env=env) != 0:
                self._log(app, "apt-get update reported errors; installing from the lists it has")
            self.apt_updated = True
        verb = "install" if action == "install" else "remove"
        if _stream([*APT_BASE, verb, "--", *app.apt_packages], on_line, env=env) != 0:
            raise RuntimeError(f"apt-get {verb} failed")
        self._progress(app, 100)

    def _flatpak(self, app: App, action: str) -> None:
        operation = flatpak_ops.install if action == "install" else flatpak_ops.uninstall
        operation(app.flatpak_id, lambda percent, message: self._progress(app, percent, message), lambda line: self._log(app, line))

    def _wait_for_debconf(self, app: App) -> None:
        if not self.debconf_locked():
            return
        self._log(app, "Waiting for another program to finish installing software (debconf is locked)")
        deadline = time.monotonic() + self.debconf_wait
        while self.debconf_locked():
            if time.monotonic() > deadline:
                raise InstallError("another program is installing software and keeps debconf locked", code="busy")
            time.sleep(DEBCONF_POLL)

    def _installed_now(self, app: App) -> bool:
        try:
            return app.id in self.installed_apps(self.catalog)
        except Exception:  # noqa: BLE001 - a failed check only means the failure stands
            return False

    def run_one(self, app: App, action: str) -> bool:
        self.dpkg_errors = DpkgErrors()
        try:
            if app.preinstalled:
                raise RuntimeError(f"{app.name} comes with CaramOS")
            method = {"recipe": self._recipe, "apt": self._apt, "flatpak": self._flatpak}[app.method]
            try:
                method(app, action)
            except Exception:
                # A package that asks a debconf question (the Microsoft fonts) fails while another program keeps
                # debconf's database locked; most apps install meanwhile, so only a failure that says so waits
                # for it, and tries once more.
                if action != "install" or not self.dpkg_errors.debconf_locked:
                    raise
                self._wait_for_debconf(app)
                self._log(app, "debconf is free again; trying once more")
                self.dpkg_errors = DpkgErrors()
                method(app, action)
        except Exception as exc:  # report and carry on with the next app
            broken = tuple(self.dpkg_errors.packages)
            # apt-get also configures every package dpkg left unfinished; one that keeps failing (an NVIDIA
            # driver whose module does not build, ticket H-1094) fails each install, though the app was installed.
            if action == "install" and broken and self._installed_now(app):
                self._log(app, f"{app.name} is installed; dpkg could not finish other packages: {', '.join(broken)}")
                self.emit("done", id=app.id, ok=True)
                return True
            fields = {}
            if isinstance(exc, InstallError) and exc.code:
                fields = {"code": exc.code, "packages": list(exc.packages)}
            elif broken:
                fields = {"code": "broken-packages", "packages": list(broken)}
            self.emit("done", id=app.id, ok=False, error=str(exc), **fields)
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
