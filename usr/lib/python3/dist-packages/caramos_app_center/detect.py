"""Find which catalog apps are already installed (flatpak list, dpkg, the Zalo state file and the launchers of the
websites)."""

from __future__ import annotations

import json
import subprocess
from collections.abc import Callable
from pathlib import Path

from . import webapp
from .catalog import App, Catalog

ZALO_STATE_FILE = Path("/var/lib/caramos-ota/zalo-appimage.json")
COMMAND_TIMEOUT = 20

Runner = Callable[[list[str]], str]


def _run(args: list[str]) -> str:
    try:
        completed = subprocess.run(args, check=False, capture_output=True, text=True, timeout=COMMAND_TIMEOUT)
    except (OSError, subprocess.TimeoutExpired):
        return ""
    return completed.stdout if completed.returncode == 0 else ""


def parse_flatpak_list(output: str) -> set[str]:
    return {line.split()[0] for line in output.splitlines() if line.strip()}


def parse_dpkg_status(output: str) -> set[str]:
    """Packages whose status is installed, from `dpkg-query -W -f='${Package}\\t${db:Status-Status}\\n'`."""

    installed = set()
    for line in output.splitlines():
        name, _, status = line.partition("\t")
        if status.strip() == "installed":
            installed.add(name.split(":")[0])
    return installed


def zalo_flavour(path: Path = ZALO_STATE_FILE) -> str:
    try:
        state = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return ""
    # Builds installed before the App Center recorded no flavour; CaramOS only shipped the light one.
    return str(state.get("flavour") or "light") if isinstance(state, dict) else ""


def installed_debs(packages: tuple[str, ...] | list[str], run: Runner = _run) -> set[str]:
    """Which of these packages are installed (one query each: dpkg-query fails as a whole on a package it does not
    know)."""

    installed: set[str] = set()
    for package in packages:
        installed |= parse_dpkg_status(run(["dpkg-query", "-W", "-f=${Package}\t${db:Status-Status}\n", "--", package]))
    return installed


def flatpak_app_installed(app_id: str, run: Runner = _run) -> bool:
    """Whether the system installation has this Flatpak app (where the App Center installs a Flathub app from a link)."""

    return app_id in parse_flatpak_list(run(["flatpak", "list", "--system", "--app", "--columns=application"]))


def flatpak_runtime_installed(runtime: str, run: Runner = _run) -> bool:
    """Whether a runtime such as org.gnome.Platform/x86_64/49 is installed, for the user or the system."""

    name, _, rest = runtime.partition("/")
    branch = rest.rpartition("/")[2]
    output = run(["flatpak", "list", "--runtime", "--columns=application,branch"])
    return any(line.split() == [name, branch] for line in output.splitlines() if line.strip())


def is_installed(app: App, flatpaks: set[str], debs: set[str], zalo: str,
                 web_dir: Path = webapp.LAUNCHER_DIR) -> bool:
    if app.method == "web":
        return webapp.is_launcher_of(webapp.launcher_path(app.id, web_dir), app.id)
    detect = app.detect or {}
    if "deb" in detect:
        return detect["deb"] in debs
    if "zalo_flavour" in detect:
        return zalo == detect["zalo_flavour"]
    if app.method == "flatpak":
        return app.flatpak_id in flatpaks
    if app.method == "apt":
        return all(package in debs for package in app.apt_packages)
    return app.preinstalled


def installed_apps(catalog: Catalog, run: Runner = _run, zalo_state: Path = ZALO_STATE_FILE,
                   web_dir: Path = webapp.LAUNCHER_DIR) -> set[str]:
    flatpaks = parse_flatpak_list(run(["flatpak", "list", "--app", "--columns=application"]))
    debs = parse_dpkg_status(run(["dpkg-query", "-W", "-f=${Package}\t${db:Status-Status}\n"]))
    zalo = zalo_flavour(zalo_state)
    return {app.id for app in catalog.apps.values() if is_installed(app, flatpaks, debs, zalo, web_dir)}
