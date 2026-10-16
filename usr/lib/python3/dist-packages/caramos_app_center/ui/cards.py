"""App cards: the cards of the first-run steps (a choice, or an installed app) and the tiles of the store grid."""

from __future__ import annotations

from collections.abc import Callable

from gi.repository import Gtk

from ..i18n import _

from ..catalog import App
from ..plan import format_size
from .common import app_icon, badge, badge_text, label, pros_cons_box, size_text, source_chip


class AppCard(Gtk.ToggleButton):
    """A choice in a first-run step. Single-choice steps group the cards like radio buttons."""

    def __init__(self, app: App) -> None:
        super().__init__()
        self.app = app
        self.installed = False
        self.add_css_class("app-card")
        self.set_valign(Gtk.Align.FILL)
        mark = Gtk.Image.new_from_icon_name("emblem-ok-symbolic")
        mark.add_css_class("check")
        footer = Gtk.Box(spacing=6)
        if badge_text(app):
            footer.append(badge(badge_text(app), app.badge))
        footer.append(source_chip(app))
        footer.append(_size_label(app))
        self.set_child(_card_body(app, mark, footer))
        self.update_property([Gtk.AccessibleProperty.LABEL], [f"{app.name}. {app.summary_text()}"])


class InstalledCard(Gtk.Box):
    """An installed app in a first-run step: not a choice. With on_remove (apps the App Center can install,
    not the ones that come with CaramOS) it has a Remove button."""

    def __init__(self, app: App, on_remove: Callable[[App], None] | None) -> None:
        super().__init__()
        self.app = app
        self.installed = True
        self.add_css_class("app-card")
        self.add_css_class("installed-card")
        self.set_valign(Gtk.Align.FILL)
        mark = Gtk.Image.new_from_icon_name("object-select-symbolic")
        mark.add_css_class("installed-mark")
        footer = Gtk.Box(spacing=6)
        footer.append(badge(_("Installed"), "installed"))
        footer.append(source_chip(app))
        spacer = Gtk.Box(hexpand=True)
        footer.append(spacer)
        if on_remove is not None:
            remove = Gtk.Button(label=_("Remove"), valign=Gtk.Align.CENTER)
            remove.add_css_class("remove-app")
            remove.set_tooltip_text(_("Remove %s from this computer") % app.name)
            remove.connect("clicked", lambda _button: on_remove(app))
            footer.append(remove)
        else:
            self.set_tooltip_text(_("%s comes with CaramOS") % app.name)
        self.append(_card_body(app, mark, footer))
        self.update_property([Gtk.AccessibleProperty.LABEL], [f"{app.name}. {_('Installed')}"])


def _card_body(app: App, mark: Gtk.Image, footer: Gtk.Box) -> Gtk.Box:
    body = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=12, hexpand=True)

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
    mark.set_pixel_size(22)
    mark.set_valign(Gtk.Align.START)
    header.append(mark)
    body.append(header)

    pros_cons = pros_cons_box(app, max_width_chars=23)
    pros_cons.set_vexpand(True)
    body.append(pros_cons)

    footer.add_css_class("card-footer")
    body.append(footer)
    return body


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
