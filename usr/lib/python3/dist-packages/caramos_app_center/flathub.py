"""What Flathub says about an app the App Center does not suggest itself, for the page an install link opens.

Flathub's public API (https://flathub.org/api/v2): /appstream/<id> gives the name, summary, developer, licence and icon,
/summary/<id> the download and installed sizes and the runtime, /verification/<id>/status whether the developer verified
the app. Everything read from it is shown as text; only the app id, already checked by links.parse_link, is ever passed
on to install. Without GTK, so the helper side and the tests can use it.
"""

from __future__ import annotations

import json
import os
import tempfile
import urllib.error
import urllib.parse
import urllib.request
from collections.abc import Callable, Mapping
from dataclasses import dataclass
from pathlib import Path

from .links import is_flathub_id

API = "https://flathub.org/api/v2"
ICON_HOST = "dl.flathub.org"
TIMEOUT = 20
MAX_JSON_BYTES = 4 * 1024 * 1024
MAX_ICON_BYTES = 1024 * 1024
# Appstream component types that are apps people open, not runtimes, add-ons, fonts or codecs.
APP_TYPES = ("desktop", "desktop-application", "console-application", "web-application")

GetJson = Callable[[str], object]


class FlathubError(Exception):
    """kind: "not-found" (Flathub has no such app), "network" (no answer: offline or Flathub down) or "unavailable"
    (an answer the App Center cannot use)."""

    def __init__(self, kind: str, message: str = "") -> None:
        super().__init__(message or kind)
        self.kind = kind


@dataclass(frozen=True)
class FlathubApp:
    id: str
    name: str
    summary: str
    developer: str
    licence: str
    free: bool | None
    icon_url: str
    homepage: str
    download_size: int
    installed_size: int
    runtime: str
    verified: bool
    end_of_life: bool


def _get_json(url: str) -> object:
    request = urllib.request.Request(url, headers={"Accept": "application/json", "User-Agent": "caramos-app-center"})
    try:
        with urllib.request.urlopen(request, timeout=TIMEOUT) as response:
            data = response.read(MAX_JSON_BYTES + 1)
    except urllib.error.HTTPError as exc:
        if exc.code == 404:
            raise FlathubError("not-found", f"{url}: 404") from exc
        raise FlathubError("unavailable", f"{url}: HTTP {exc.code}") from exc
    except (urllib.error.URLError, OSError) as exc:
        raise FlathubError("network", str(exc)) from exc
    if len(data) > MAX_JSON_BYTES:
        raise FlathubError("unavailable", f"{url}: answer too large")
    try:
        return json.loads(data)
    except ValueError as exc:
        raise FlathubError("unavailable", f"{url}: not JSON") from exc


def _text(value: object, limit: int = 300) -> str:
    return " ".join(str(value).split())[:limit] if isinstance(value, (str, int, float)) else ""


def _size(value: object) -> int:
    return value if isinstance(value, int) and value >= 0 else 0


def icon_url_ok(url: str) -> bool:
    """Only icons from Flathub's own media server, over https."""

    try:
        parts = urllib.parse.urlsplit(url)
    except ValueError:
        return False
    return parts.scheme == "https" and parts.hostname == ICON_HOST and not parts.username and parts.port is None


def fetch_app(app_id: str, get_json: GetJson = _get_json) -> FlathubApp:
    if not is_flathub_id(app_id):
        raise FlathubError("not-found", f"not an app id: {app_id!r}")
    quoted = urllib.parse.quote(app_id, safe="")
    appstream = get_json(f"{API}/appstream/{quoted}")
    if not isinstance(appstream, Mapping) or appstream.get("id") != app_id:
        raise FlathubError("not-found", f"{app_id}: no appstream")
    if appstream.get("type") not in APP_TYPES:
        # A runtime, an extension or a font: not something to open from the menu.
        raise FlathubError("not-found", f"{app_id}: a {appstream.get('type')!r}, not an app")
    summary = get_json(f"{API}/summary/{quoted}")
    if not isinstance(summary, Mapping):
        raise FlathubError("unavailable", f"{app_id}: no summary")
    metadata = summary.get("metadata") if isinstance(summary.get("metadata"), Mapping) else {}
    try:
        verification = get_json(f"{API}/verification/{quoted}/status")
    except FlathubError:
        verification = {}   # only a nicety: the page says nothing about it then
    urls = appstream.get("urls") if isinstance(appstream.get("urls"), Mapping) else {}
    homepage = _text(urls.get("homepage"), 500)
    icon = _text(appstream.get("icon"), 500)
    free = appstream.get("is_free_license")
    return FlathubApp(
        id=app_id,
        name=_text(appstream.get("name"), 80) or app_id,
        summary=_text(appstream.get("summary")),
        developer=_text(appstream.get("developer_name"), 120),
        licence=_text(appstream.get("project_license"), 200),
        free=free if isinstance(free, bool) else None,
        icon_url=icon if icon_url_ok(icon) else "",
        homepage=homepage if homepage.startswith("https://") or homepage.startswith("http://") else "",
        download_size=_size(summary.get("download_size")),
        installed_size=_size(summary.get("installed_size")),
        runtime=_text(metadata.get("runtime"), 200),
        verified=isinstance(verification, Mapping) and verification.get("verified") is True,
        end_of_life=bool(appstream.get("is_eol")),
    )


def fetch_icon(app: FlathubApp, directory: Path, opener=urllib.request.urlopen) -> Path | None:
    """The app's icon, kept in directory (the user's cache) as <id>.png; None when there is none or it does not come."""

    if not app.icon_url or not icon_url_ok(app.icon_url):
        return None
    target = directory / f"{app.id}.png"
    request = urllib.request.Request(app.icon_url, headers={"User-Agent": "caramos-app-center"})
    try:
        with opener(request, timeout=TIMEOUT) as response:
            data = response.read(MAX_ICON_BYTES + 1)
    except (urllib.error.URLError, OSError):
        return target if target.is_file() else None
    if len(data) > MAX_ICON_BYTES or not data.startswith(b"\x89PNG\r\n\x1a\n"):
        return None
    directory.mkdir(parents=True, exist_ok=True)
    fd, name = tempfile.mkstemp(prefix=f".{target.name}.", dir=directory)
    try:
        with os.fdopen(fd, "wb") as handle:
            handle.write(data)
        os.replace(name, target)
    except BaseException:
        Path(name).unlink(missing_ok=True)
        raise
    return target
