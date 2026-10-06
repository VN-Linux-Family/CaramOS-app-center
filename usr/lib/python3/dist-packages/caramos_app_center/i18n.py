"""Translations for the App Center.

Source strings are English; Vietnamese comes from po/caramos-app-center/vi.po, compiled at package build
time to /usr/share/locale/vi/LC_MESSAGES/caramos-app-center.mo. The language follows the session the
program runs in (LANGUAGE, LC_ALL, LC_MESSAGES, LANG), as chosen in Settings > Languages.
"""

from __future__ import annotations

import gettext
import os
from collections.abc import Mapping

DOMAIN = "caramos-app-center"
LOCALE_DIR = os.environ.get("CARAMOS_APP_CENTER_LOCALEDIR", "/usr/share/locale")

_translation = gettext.translation(DOMAIN, localedir=LOCALE_DIR, fallback=True)


def _(message: str) -> str:
    return _translation.gettext(message)


def ngettext(singular: str, plural: str, count: int) -> str:
    return _translation.ngettext(singular, plural, count)


def ui_language(environ: Mapping[str, str] | None = None) -> str:
    """Language code gettext uses for this session, e.g. "vi" or "en"."""

    env = os.environ if environ is None else environ
    for key in ("LANGUAGE", "LC_ALL", "LC_MESSAGES", "LANG"):
        value = env.get(key, "").strip()
        if not value:
            continue
        code = value.split(":")[0].split(".")[0].split("@")[0].split("_")[0]
        if code and code not in {"C", "POSIX"}:
            return code
        if key != "LANGUAGE":
            break
    return "en"


def prefers_vietnamese(environ: Mapping[str, str] | None = None) -> bool:
    return ui_language(environ) == "vi"
