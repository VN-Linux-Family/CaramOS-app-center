"""Turn a selection of app IDs into what the helper has to install, with a size estimate."""

from __future__ import annotations

from collections.abc import Iterable
from dataclasses import dataclass

from .i18n import prefers_vietnamese

from .catalog import App, Catalog

# The Flathub runtime (org.freedesktop.Platform with its GL and codecs extensions) that the first
# Flatpak app pulls in, measured on CaramOS 1.0.19 with ONLYOFFICE and LibreOffice.
FLATPAK_RUNTIME_MB = 1100


@dataclass(frozen=True)
class Plan:
    install: tuple[App, ...]
    size_mb: int
    runtime_mb: int

    @property
    def ids(self) -> list[str]:
        return [app.id for app in self.install]

    @property
    def total_mb(self) -> int:
        return self.size_mb + self.runtime_mb


def make_plan(catalog: Catalog, selected: Iterable[str], installed: set[str]) -> Plan:
    wanted = set(selected)
    apps = tuple(
        app for app in catalog.apps.values()
        if app.id in wanted and app.id not in installed and not app.preinstalled
    )
    has_flatpak_installed = any(catalog.apps[app_id].method == "flatpak" for app_id in installed if app_id in catalog.apps)
    needs_runtime = any(app.method == "flatpak" for app in apps) and not has_flatpak_installed
    return Plan(
        install=apps,
        size_mb=sum(app.size_mb for app in apps),
        runtime_mb=FLATPAK_RUNTIME_MB if needs_runtime else 0,
    )


def default_selection(catalog: Catalog, installed: set[str]) -> dict[str, set[str]]:
    """Per step, the defaults that still need installing; a single-choice step keeps an installed pick."""

    selection: dict[str, set[str]] = {}
    for step in catalog.steps:
        if step.choice == "single" and any(app_id in installed for app_id in step.apps):
            selection[step.id] = set()
            continue
        selection[step.id] = {app_id for app_id in step.default if app_id not in installed}
    return selection


def format_size(mb: int) -> str:
    if mb < 1000:
        return f"{mb} MB"
    text = f"{mb / 1000:.1f}".removesuffix(".0")
    return f"{text.replace('.', ',') if prefers_vietnamese() else text} GB"
