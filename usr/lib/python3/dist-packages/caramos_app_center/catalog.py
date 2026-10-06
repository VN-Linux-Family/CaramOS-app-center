"""The App Center catalog: which apps are offered, how each one is installed and the first-run steps.

The catalog is a root-owned JSON file shipped in the package. The privileged helper only accepts app
IDs and looks up how to install them here, so nothing a user can edit decides what runs as root.
Display text carries Vietnamese and English ({"vi": ..., "en": ...}); the session language picks one.
"""

from __future__ import annotations

import json
import os
import re
import urllib.parse
from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path

from .i18n import prefers_vietnamese

DATA_DIR = Path("/usr/share/caramos-app-center")
# The App Center only suggests basic apps; everything else is on the VNLF app site.
MORE_APPS_URL = "https://apps.vietnamlinuxfamily.net/browse"
CATALOG_FILE = DATA_DIR / "catalog.json"
RECIPE_DIR = DATA_DIR / "recipes"
ICON_DIR = DATA_DIR / "icons"

ID_RE = re.compile(r"^[a-z0-9][a-z0-9-]{0,40}$")
FLATPAK_RE = re.compile(r"^[A-Za-z][A-Za-z0-9_-]*(\.[A-Za-z0-9_-]+){2,}$")
DEB_RE = re.compile(r"^[a-z0-9][a-z0-9+.-]+$")
METHODS = ("flatpak", "recipe", "apt", "preinstalled")
CHOICES = ("single", "multi")
BADGES = ("recommended", "popular")
DETECT_KEYS = ("deb", "zalo_flavour")


class CatalogError(ValueError):
    """Raised when the catalog file is malformed."""


def localized(value: object, environ: Mapping[str, str] | None = None) -> str:
    """Pick the session language from a {"vi": ..., "en": ...} text."""

    if isinstance(value, str):
        return value
    if not isinstance(value, Mapping):
        return ""
    key = "vi" if prefers_vietnamese(environ) else "en"
    return str(value.get(key) or value.get("en") or value.get("vi") or "")


def localized_list(value: object, environ: Mapping[str, str] | None = None) -> list[str]:
    if not isinstance(value, Mapping):
        return []
    key = "vi" if prefers_vietnamese(environ) else "en"
    items = value.get(key) or value.get("en") or []
    return [str(item) for item in items]


@dataclass(frozen=True)
class App:
    id: str
    name: str
    category: str
    method: str
    targets: tuple[str, ...]
    summary: Mapping[str, str]
    pros: Mapping[str, list[str]]
    cons: Mapping[str, list[str]]
    size_mb: int
    website: str = ""
    icon: str = ""
    icon_name: str = ""
    badge: str = ""
    detect: Mapping[str, str] | None = None

    @property
    def flatpak_id(self) -> str:
        return self.targets[0] if self.method == "flatpak" else ""

    @property
    def recipe(self) -> str:
        return self.targets[0] if self.method == "recipe" else ""

    @property
    def apt_packages(self) -> tuple[str, ...]:
        return self.targets if self.method == "apt" else ()

    @property
    def preinstalled(self) -> bool:
        return self.method == "preinstalled"

    def summary_text(self, environ: Mapping[str, str] | None = None) -> str:
        return localized(self.summary, environ)

    def pros_list(self, environ: Mapping[str, str] | None = None) -> list[str]:
        return localized_list(self.pros, environ)

    def cons_list(self, environ: Mapping[str, str] | None = None) -> list[str]:
        return localized_list(self.cons, environ)

    def icon_path(self, icon_dir: Path = ICON_DIR) -> Path | None:
        return icon_dir / self.icon if self.icon else None


@dataclass(frozen=True)
class Step:
    id: str
    title: Mapping[str, str]
    subtitle: Mapping[str, str]
    choice: str
    allow_none: bool
    apps: tuple[str, ...]
    default: tuple[str, ...]


@dataclass(frozen=True)
class Category:
    id: str
    title: Mapping[str, str]
    icon: str


@dataclass(frozen=True)
class Catalog:
    apps: dict[str, App]
    steps: tuple[Step, ...]
    categories: tuple[Category, ...]

    def app(self, app_id: str) -> App:
        return self.apps[app_id]

    def apps_in(self, category: str) -> list[App]:
        return [app for app in self.apps.values() if app.category == category]


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise CatalogError(message)


def _bilingual(raw: Mapping[str, object], key: str, where: str, *, is_list: bool = False) -> dict:
    value = raw.get(key)
    _require(isinstance(value, Mapping), f"{where}: {key} must have vi and en")
    for language in ("vi", "en"):
        item = value.get(language)
        if is_list:
            _require(isinstance(item, list) and item and all(isinstance(x, str) and x.strip() for x in item),
                     f"{where}: {key}.{language} must be a non-empty list of text")
        else:
            _require(isinstance(item, str) and bool(item.strip()), f"{where}: {key}.{language} is empty")
    return dict(value)


def _parse_app(raw: Mapping[str, object], recipe_dir: Path | None) -> App:
    app_id = str(raw.get("id", ""))
    where = f"app {app_id or '?'}"
    _require(bool(ID_RE.fullmatch(app_id)), f"{where}: invalid id")
    install = raw.get("install")
    _require(isinstance(install, Mapping) and len(install) == 1 and next(iter(install)) in METHODS,
             f"{where}: install needs exactly one of {', '.join(METHODS)}")
    method, value = next(iter(install.items()))
    if method == "flatpak":
        _require(isinstance(value, str) and bool(FLATPAK_RE.fullmatch(value)), f"{where}: invalid flatpak id")
        targets = (value,)
    elif method == "recipe":
        _require(isinstance(value, str) and bool(ID_RE.fullmatch(value)), f"{where}: invalid recipe name")
        if recipe_dir is not None:
            recipe = recipe_dir / value
            _require(recipe.is_file() and os.access(recipe, os.X_OK), f"{where}: recipe {recipe} is not executable")
        targets = (value,)
    elif method == "apt":
        _require(isinstance(value, list) and value and all(isinstance(p, str) and DEB_RE.fullmatch(p) for p in value),
                 f"{where}: apt needs package names")
        targets = tuple(value)
    else:
        _require(value is True, f"{where}: preinstalled must be true")
        targets = ()
    detect = raw.get("detect")
    if detect is not None:
        _require(isinstance(detect, Mapping) and len(detect) == 1 and next(iter(detect)) in DETECT_KEYS,
                 f"{where}: detect needs one of {', '.join(DETECT_KEYS)}")
    _require(method == "flatpak" or detect is not None or method == "apt", f"{where}: needs detect to know when it is installed")
    badge = str(raw.get("badge") or "")
    _require(badge in ("", *BADGES), f"{where}: unknown badge {badge}")
    _require(bool(raw.get("icon") or raw.get("icon_name")), f"{where}: needs icon or icon_name")
    size = raw.get("size_mb")
    _require(isinstance(size, int) and size > 0, f"{where}: size_mb must be a positive integer")
    return App(
        id=app_id,
        name=str(raw.get("name") or app_id),
        category=str(raw.get("category") or ""),
        method=method,
        targets=targets,
        summary=_bilingual(raw, "summary", where),
        pros=_bilingual(raw, "pros", where, is_list=True),
        cons=_bilingual(raw, "cons", where, is_list=True),
        size_mb=size,
        website=str(raw.get("website") or ""),
        icon=str(raw.get("icon") or ""),
        icon_name=str(raw.get("icon_name") or ""),
        badge=badge,
        detect=dict(detect) if detect else None,
    )


def parse_catalog(data: Mapping[str, object], recipe_dir: Path | None = None) -> Catalog:
    _require(data.get("schema") == 1, "catalog schema must be 1")
    apps: dict[str, App] = {}
    for raw in data.get("apps") or []:
        _require(isinstance(raw, Mapping), "apps must be objects")
        app = _parse_app(raw, recipe_dir)
        _require(app.id not in apps, f"duplicate app id {app.id}")
        apps[app.id] = app

    categories = []
    for raw in data.get("categories") or []:
        category_id = str(raw.get("id", ""))
        _require(bool(ID_RE.fullmatch(category_id)), f"invalid category id {category_id!r}")
        categories.append(Category(category_id, _bilingual(raw, "title", f"category {category_id}"), str(raw.get("icon") or "")))
    known = {category.id for category in categories}
    for app in apps.values():
        _require(app.category in known, f"app {app.id}: unknown category {app.category}")

    steps = []
    for raw in data.get("steps") or []:
        step_id = str(raw.get("id", ""))
        where = f"step {step_id}"
        _require(bool(ID_RE.fullmatch(step_id)), f"{where}: invalid id")
        choice = raw.get("choice")
        _require(choice in CHOICES, f"{where}: choice must be single or multi")
        step_apps = tuple(raw.get("apps") or ())
        default = tuple(raw.get("default") or ())
        _require(bool(step_apps), f"{where}: needs apps")
        for app_id in step_apps:
            _require(app_id in apps, f"{where}: unknown app {app_id}")
        _require(set(default) <= set(step_apps), f"{where}: default must be among its apps")
        _require(choice == "multi" or len(default) <= 1, f"{where}: single choice allows one default")
        steps.append(Step(step_id, _bilingual(raw, "title", where), _bilingual(raw, "subtitle", where), choice,
                          bool(raw.get("allow_none")), step_apps, default))
    return Catalog(apps=apps, steps=tuple(steps), categories=tuple(categories))


def load_catalog(path: Path = CATALOG_FILE, recipe_dir: Path | None = RECIPE_DIR) -> Catalog:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        raise CatalogError(f"cannot read {path}: {exc}") from exc
    _require(isinstance(data, Mapping), f"{path}: not a JSON object")
    return parse_catalog(data, recipe_dir)


def more_apps_url(query: str = "") -> str:
    """The VNLF app site, searching for query when given."""

    query = query.strip()
    return f"{MORE_APPS_URL}?{urllib.parse.urlencode({'q': query})}" if query else MORE_APPS_URL


def data_dir() -> Path:
    """Where the UI reads catalog, icons and style; overridable for development only."""

    override = os.environ.get("CARAMOS_APP_CENTER_DATA")
    return Path(override) if override else DATA_DIR
