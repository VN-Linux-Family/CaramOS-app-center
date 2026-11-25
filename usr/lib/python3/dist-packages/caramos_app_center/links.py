"""Install links from the VNLF app site: caramos-app://install/<Flathub app id>.

The site's "Cài trên CaramOS" button opens such a link; caramos-app-center-link.desktop hands it to the App Center
(MimeType=x-scheme-handler/caramos-app). Nothing from a link is used but the app id, and only once it is a valid Flatpak
application id: the App Center then shows that app's page (its own card for the apps it suggests), and installs it, from
Flathub only, when the user presses Install.
"""

from __future__ import annotations

import re
from dataclasses import dataclass

SCHEME = "caramos-app"
PREFIX = f"{SCHEME}://install/"
MAX_LENGTH = 300
# Flatpak's rules for an application id (flatpak_is_valid_name): three parts or more separated by dots, each made of
# letters, digits and underscores and not starting with a digit; only the last part may also contain dashes; 255
# characters at most.
FLATHUB_ID_RE = re.compile(r"^(?=.{5,255}$)[A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)+\.[A-Za-z_][A-Za-z0-9_-]*$")


class LinkError(ValueError):
    """The text is not an install link the App Center opens."""


@dataclass(frozen=True)
class InstallLink:
    app_id: str


def is_flathub_id(text: object) -> bool:
    return isinstance(text, str) and bool(FLATHUB_ID_RE.fullmatch(text))


def parse_link(text: object) -> InstallLink:
    """caramos-app://install/<app id>, with nothing else: no query, fragment, escapes or other path. The scheme and the
    word install may come in capitals (browsers do not change them, but people type links)."""

    if not isinstance(text, str) or not text or len(text) > MAX_LENGTH:
        raise LinkError("not an install link")
    if not text.isascii() or any(char.isspace() or not char.isprintable() for char in text):
        raise LinkError("not an install link")
    if text[:len(PREFIX)].lower() != PREFIX:
        raise LinkError("not an install link")
    app_id = text[len(PREFIX):]
    if app_id.endswith("/"):
        app_id = app_id[:-1]
    if not is_flathub_id(app_id):
        raise LinkError("not a valid app id")
    return InstallLink(app_id)
