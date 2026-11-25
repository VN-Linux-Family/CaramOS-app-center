"""Websites as apps: a launcher in the menu that opens one website in its own window.

A catalog app installed with {"web": {"url": ..., "hosts": [...], "block_ads": true}} downloads nothing. The root
helper writes its launcher, /usr/share/applications/caramos-web-<id>.desktop, from the root-owned catalog; the launcher
runs caramos-web-app with the app's id only, and the window (webview.py, WebKitGTK, which both CaramOS editions have)
reads the address and the site's hosts from the same catalog.

The window shows that site and nothing else: a link the user follows to another site opens in the default browser,
pop-up windows the page opens by itself are dropped, nothing is downloaded and the page gets no camera, microphone,
location or notifications. With block_ads, the ad and tracker servers in web-blocklist.txt are never contacted, for sites
whose terms do not forbid it. A site whose terms allow no window of ours, nor blocking ("open": "browser": Polypad, whose
Amplify terms forbid framing it and blocking or obscuring any portion of it) gets the same launcher, which hands the
address to the user's web browser and opens no window.

This module is the part without GTK, so the helper and the tests can use it.
"""

from __future__ import annotations

import os
import re
import sys
import tempfile
import urllib.parse
from collections.abc import Iterable, Mapping
from pathlib import Path
from typing import TYPE_CHECKING

from . import renderer

if TYPE_CHECKING:  # catalog imports this module
    from .catalog import App

LAUNCHER_DIR = Path("/usr/share/applications")
RUNNER = "/usr/lib/caramos-app-center/caramos-web-app"
BLOCKLIST_FILE = Path("/usr/share/caramos-app-center/web-blocklist.txt")
ICON_DIR = Path("/usr/share/caramos-app-center/icons")
# The window is GTK 3 with WebKitGTK 4.1: both CaramOS editions ship them (yelp, the installer, NetworkManager's VPN
# sign-in use them). The helper installs the bindings if someone removed them.
RUNTIME_PACKAGES = ("gir1.2-webkit2-4.1",)
HOST_RE = re.compile(r"^(?=.{1,253}$)(?:[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\.)+[a-z]{2,63}$")
# Menu sections for the catalog categories; anything else goes with the network programs.
MENU_CATEGORIES = {"education": "Education;", "graphics": "Graphics;", "office": "Office;", "media": "AudioVideo;"}
# Schemes a link may hand to the default browser or mail program; anything else (file:, javascript:, intent:) is dropped.
EXTERNAL_SCHEMES = ("https", "http", "mailto")

STAY, BROWSER, DROP, HERE = "stay", "browser", "drop", "here"
# WebKit draws pages with OpenGL. Where the App Center has GTK draw with cairo (renderer.py: no graphics card with a
# driver, or 4 GiB of memory or less), the window turns WebKit's accelerated compositing off too. Measured on the 2 GB
# Xfce VM (2026-11-25), TheColor.com's home page: 264 MB instead of 495 MB (PSS of the three processes), 9 % of the CPU
# instead of 29 %. A WebKit variable the user set stays.
WEBKIT_KEYS = ("WEBKIT_DISABLE_COMPOSITING_MODE", "WEBKIT_DISABLE_DMABUF_RENDERER", "WEBKIT_SKIA_ENABLE_CPU_RENDERING")


def open_in_browser(url: str) -> bool:
    """Hand url to the user's default web browser (for the sites that open there, not in a window of ours)."""

    import gi

    gi.require_version("Gio", "2.0")
    from gi.repository import Gio, GLib

    try:
        return Gio.AppInfo.launch_default_for_uri(url, None)
    except GLib.Error as exc:
        print(f"caramos-web-app: cannot open {url}: {exc.message}", file=sys.stderr)
        return False


def launcher_path(app_id: str, directory: Path = LAUNCHER_DIR) -> Path:
    return directory / f"caramos-web-{app_id}.desktop"


def window_class(app_id: str) -> str:
    """The window's class (StartupWMClass): the taskbar shows the launcher's icon and name for it."""

    return f"caramos-web-{app_id}"


def application_id(app_id: str) -> str:
    """One window per site: opening the launcher again brings the open window forward."""

    return "net.vietnamlinuxfamily.CaramOSWebApp." + app_id.replace("-", "_")


def exec_line(app_id: str) -> str:
    return f"Exec={RUNNER} {app_id}"


def webkit_environment(environ: Mapping[str, str], drivers: set[str], memory: int | None) -> dict[str, str]:
    """The variables to set before WebKit starts its processes."""

    if any(environ.get(key) for key in WEBKIT_KEYS):
        return {}
    return {"WEBKIT_DISABLE_COMPOSITING_MODE": "1"} if renderer.renderer({}, drivers, memory) == "cairo" else {}


# Launchers ---------------------------------------------------------------------------------------------------


def _desktop_value(text: str) -> str:
    """A desktop entry value on one line (the catalog is root-owned, but a stray newline must not add a key)."""

    return " ".join(str(text).replace("\\", "\\\\").split())


def launcher_text(app: "App") -> str:
    icon = str(ICON_DIR / app.icon) if app.icon else (app.icon_name or "applications-internet")
    summary = app.summary
    lines = [
        "[Desktop Entry]",
        "Type=Application",
        f"Name={_desktop_value(app.name)}",
        f"Comment={_desktop_value(summary.get('en') or summary.get('vi') or '')}",
        f"Comment[vi]={_desktop_value(summary.get('vi') or summary.get('en') or '')}",
        exec_line(app.id),
        f"Icon={_desktop_value(icon)}",
        "Terminal=false",
        f"Categories={MENU_CATEGORIES.get(app.category, 'Network;')}",
    ]
    if app.web_window:
        lines += ["StartupNotify=true", f"StartupWMClass={window_class(app.id)}"]
    else:
        # No window of ours: the browser's own window shows up, so no busy cursor waiting for one.
        lines.append("StartupNotify=false")
    # The App Center installed it and removes it; menu editors must not treat it as the user's own.
    lines.append("X-CaramOS-App-Center=web")
    return "\n".join(lines) + "\n"


def is_launcher_of(path: Path, app_id: str) -> bool:
    """Whether path is the launcher this module writes for app_id (and not a file someone put there)."""

    try:
        text = path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError):
        return False
    return exec_line(app_id) in text.splitlines()


def write_launcher(app: "App", directory: Path = LAUNCHER_DIR) -> Path:
    target = launcher_path(app.id, directory)
    directory.mkdir(parents=True, exist_ok=True)
    fd, name = tempfile.mkstemp(prefix=f".{target.name}.", dir=directory)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as handle:
            handle.write(launcher_text(app))
        os.chmod(name, 0o644)
        os.replace(name, target)
    except BaseException:
        Path(name).unlink(missing_ok=True)
        raise
    return target


def remove_launcher(app: "App", directory: Path = LAUNCHER_DIR) -> bool:
    """Remove the app's launcher; one that is not ours stays. True when no launcher of ours is left."""

    target = launcher_path(app.id, directory)
    if not target.exists():
        return True
    if not is_launcher_of(target, app.id):
        return False
    target.unlink()
    return True


# Where a navigation goes ---------------------------------------------------------------------------------------


def host_matches(host: str, hosts: Iterable[str]) -> bool:
    host = (host or "").lower().rstrip(".")
    return any(host == allowed or host.endswith("." + allowed) for allowed in hosts)


def is_allowed(url: str, hosts: Iterable[str]) -> bool:
    """Whether url is a page of the site: https on one of its hosts or their subdomains, no user name in it."""

    try:
        parts = urllib.parse.urlsplit(url)
    except ValueError:
        return False
    if parts.scheme != "https" or parts.username is not None or parts.password is not None:
        return False
    try:
        host = parts.hostname or ""
    except ValueError:
        return False
    return host_matches(host, hosts)


def _scheme(url: str) -> str:
    try:
        return urllib.parse.urlsplit(url).scheme.lower()
    except ValueError:
        return ""


def navigation_choice(url: str, hosts: Iterable[str], *, user_action: bool) -> str:
    """For a navigation of the window: STAY to load it, BROWSER to give it to the default browser, DROP to ignore it.

    Only what the user did leaves the window: a click on a link to another site opens in the browser. What a page does
    by itself (redirects, frames, scripts) is the site's own business and stays, except a non-web address it tries to
    open (a program, a file) which is dropped."""

    if is_allowed(url, hosts):
        return STAY
    scheme = _scheme(url)
    if user_action:
        return BROWSER if scheme in EXTERNAL_SCHEMES else DROP
    if scheme in ("https", "http", "about", "data", "blob"):
        return STAY
    return DROP


def new_window_choice(url: str, hosts: Iterable[str], *, user_action: bool) -> str:
    """For a new window the page asks for: HERE to load it in the same window, BROWSER, or DROP.

    A window the page opens without a click is a pop-up: always dropped."""

    if not user_action:
        return DROP
    if is_allowed(url, hosts):
        return HERE
    return BROWSER if _scheme(url) in EXTERNAL_SCHEMES else DROP


# Ad and tracker blocking ---------------------------------------------------------------------------------------


def parse_blocklist(text: str) -> list[str]:
    """Host names, one per line; # starts a comment. A host blocks its subdomains too."""

    domains: list[str] = []
    for line in text.splitlines():
        name = line.split("#", 1)[0].strip().lower()
        if not name:
            continue
        if not HOST_RE.fullmatch(name):
            raise ValueError(f"not a host name: {name!r}")
        if name not in domains:
            domains.append(name)
    return domains


def load_blocklist(path: Path = BLOCKLIST_FILE) -> list[str]:
    return parse_blocklist(path.read_text(encoding="utf-8"))


def content_rules(domains: Iterable[str]) -> list[dict]:
    """WebKit content-blocker rules (the JSON format of Safari's content blockers): every request to these hosts or
    their subdomains is blocked, whatever loads it."""

    rules = []
    for domain in domains:
        pattern = "^[^:]+://+([^:/]+\\.)?" + domain.replace(".", "\\.") + "[:/]"
        rules.append({"trigger": {"url-filter": pattern}, "action": {"type": "block"}})
    return rules


def blocked_hosts_in(hosts: Iterable[str], domains: Iterable[str]) -> list[str]:
    """The site's own hosts that the block list would block (must be none)."""

    domains = list(domains)
    return [host for host in hosts if host_matches(host, domains)]
