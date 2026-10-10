"""App cards: the selectable cards of the first-run steps and the tiles of the store grid."""

from __future__ import annotations

from collections.abc import Callable

from gi.repository import Gtk

from ..i18n import _

from ..catalog import App
from ..plan import format_size
from .common import app_icon, badge, badge_text, label, pros_cons_box, size_text, source_chip


class AppCard(Gtk.ToggleButton):
    """A choice in a first-run step. Single-choice steps group the cards like radio buttons."""

    def __init__(self, app: App, installed: bool) -> None:
        super().__init__()
        self.app = app
        self.installed = installed
        self.add_css_class("app-card")
        self.set_valign(Gtk.Align.FILL)

        body = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=12)

        # Labels get a bounded natural width so the step grids fit three cards per row.
        header = Gtk.Box(spacing=12)
        header.append(app_icon(app, 48))
        titles = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2, valign=Gtk.Align.CENTER, hexpand=True)
        name = label(app.name, "app-name")
        name.set_max_width_chars(14)
        titles.append(name)
        summary = label(app.summary_text(), "app-summary", "dim-label")
        summary.set_max_width_chars(20)
        titles.append(summary)
        header.append(titles)
        mark = Gtk.Image.new_from_icon_name("emblem-ok-symbolic" if not installed else "object-select-symbolic")
        mark.set_pixel_size(22)
        mark.add_css_class("installed-mark" if installed else "check")
        mark.set_valign(Gtk.Align.START)
        header.append(mark)
        body.append(header)

        pros_cons = pros_cons_box(app, max_width_chars=23)
        pros_cons.set_vexpand(True)
        body.append(pros_cons)

        footer = Gtk.Box(spacing=6)
        footer.add_css_class("card-footer")
        if installed:
            footer.append(badge(_("Installed"), "installed"))
        elif badge_text(app):
            footer.append(badge(badge_text(app), app.badge))
        footer.append(source_chip(app))
        footer.append(_size_label(app))
        body.append(footer)

        self.set_child(body)
        self.update_property([Gtk.AccessibleProperty.LABEL], [f"{app.name}. {app.summary_text()}"])
        if installed:
            self.add_css_class("installed")
            self.set_sensitive(False)
            self.set_tooltip_text(_("%s is already installed") % app.name)


def _size_label(app: App) -> Gtk.Label:
    # Bare size: the footer must stay narrow enough for three cards per row.
    widget = label(format_size(app.size_mb), "dim-label", wrap=False, xalign=1.0)
    widget.set_tooltip_text(size_text(app))
    widget.set_hexpand(True)
    return widget


class NoneCard(Gtk.ToggleButton):
    """'Not now' choice of a single-choice step that allows none."""

    def __init__(self, text: str) -> None:
        super().__init__()
        self.app = None
        self.installed = False
        self.add_css_class("app-card")
        self.add_css_class("none-card")
        row = Gtk.Box(spacing=12)
        icon = Gtk.Image.new_from_icon_name("action-unavailable-symbolic")
        icon.set_pixel_size(20)
        row.append(icon)
        row.append(label(text, wrap=False))
        mark = Gtk.Image.new_from_icon_name("emblem-ok-symbolic")
        mark.add_css_class("check")
        mark.set_hexpand(True)
        mark.set_halign(Gtk.Align.END)
        row.append(mark)
        self.set_child(row)


class StoreTile(Gtk.Button):
    """A compact app tile in the store grid; activating it opens the detail page."""

    def __init__(self, app: App, installed: bool, on_open: Callable[[App], None]) -> None:
        super().__init__()
        self.app = app
        self.add_css_class("store-tile")
        row = Gtk.Box(spacing=14)
        row.append(app_icon(app, 52))
        text = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4, valign=Gtk.Align.CENTER, hexpand=True)
        name_row = Gtk.Box(spacing=8)
        name_row.append(label(app.name, "heading", wrap=False))
        if installed:
            name_row.append(badge(_("Installed"), "installed"))
        text.append(name_row)
        summary = label(app.summary_text(), "dim-label", lines=2)
        # Bound the natural width so the grid fits two or three tiles per row.
        summary.set_max_width_chars(28)
        text.append(summary)
        row.append(text)
        self.set_child(row)
        self.update_property([Gtk.AccessibleProperty.LABEL], [f"{app.name}. {app.summary_text()}"])
        self.connect("clicked", lambda _button: on_open(app))
