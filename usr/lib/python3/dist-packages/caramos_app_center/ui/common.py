"""Small widget helpers shared by the App Center pages."""

from __future__ import annotations

from pathlib import Path

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")
from gi.repository import Adw, Gdk, Gtk, Pango  # noqa: E402

from ..i18n import _  # noqa: E402

from ..catalog import App, data_dir, more_apps_url  # noqa: E402
from ..plan import format_size  # noqa: E402

SYSTEM_LOGO = Path("/usr/share/pixmaps/caramos-logo.png")

def open_more_apps(parent: Gtk.Widget, query: str = "", category: str = "") -> None:
    """Open the VNLF app site in the default browser, searching for query or showing a category."""

    root = parent.get_root()
    Gtk.UriLauncher.new(more_apps_url(query, category)).launch(
        root if isinstance(root, Gtk.Window) else None, None, None, None)


def logo_path() -> Path:
    """The CaramOS logo; next to the data directory when running from the source tree."""

    if SYSTEM_LOGO.is_file():
        return SYSTEM_LOGO
    return data_dir().parent / "pixmaps" / "caramos-logo.png"


def load_css() -> None:
    display = Gdk.Display.get_default()
    base = Gtk.CssProvider()
    base.load_from_path(str(data_dir() / "style.css"))
    Gtk.StyleContext.add_provider_for_display(display, base, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
    dark = Gtk.CssProvider()
    dark.load_from_path(str(data_dir() / "style-dark.css"))
    manager = Adw.StyleManager.get_default()

    def apply(*_args) -> None:
        if manager.get_dark():
            Gtk.StyleContext.add_provider_for_display(display, dark, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION + 1)
        else:
            Gtk.StyleContext.remove_provider_for_display(display, dark)

    manager.connect("notify::dark", apply)
    apply()


def label(text: str, *classes: str, wrap: bool = True, xalign: float = 0.0, lines: int = 0) -> Gtk.Label:
    widget = Gtk.Label(label=text, xalign=xalign)
    widget.set_wrap(wrap)
    widget.set_wrap_mode(Pango.WrapMode.WORD_CHAR)
    if lines:
        widget.set_lines(lines)
        widget.set_ellipsize(Pango.EllipsizeMode.END)
    for css_class in classes:
        widget.add_css_class(css_class)
    return widget


def app_icon(app: App, size: int) -> Gtk.Image:
    path = app.icon_path(data_dir() / "icons")
    image = Gtk.Image.new_from_file(str(path)) if path and path.is_file() else Gtk.Image.new_from_icon_name(
        app.icon_name or "application-x-executable")
    image.set_pixel_size(size)
    return image


def logo(size: int) -> Gtk.Widget:
    path = logo_path()
    if path.is_file():
        image = Gtk.Image.new_from_file(str(path))
    else:
        image = Gtk.Image.new_from_icon_name("system-software-install")
    image.set_pixel_size(size)
    return image


def badge(text: str, *classes: str) -> Gtk.Label:
    widget = label(text, "badge", *classes, wrap=False)
    widget.set_valign(Gtk.Align.CENTER)
    return widget


def badge_text(app: App) -> str:
    return {"recommended": _("Recommended"), "popular": _("Popular")}.get(app.badge, "")


def source_text(app: App) -> str:
    return {
        "flathub": _("Flathub"),
        "ubuntu": _("Ubuntu repository"),
        "caramos": _("Comes with CaramOS"),
        "community": _("Community package"),
        "developer": _("From the developer"),
    }[app.source]


def source_chip(app: App) -> Gtk.Label:
    chip = label(source_text(app), "chip", wrap=False)
    if app.source == "flathub":
        chip.add_css_class("verified")
    chip.set_tooltip_text({
        "flathub": _("Verified app from Flathub, installed in a sandbox"),
        "ubuntu": _("Package from the Ubuntu archive"),
        "caramos": _("Already part of CaramOS"),
        "community": _("Linux build packaged by the community, not by the app's developer"),
        "developer": _("Downloaded from the developer's website and installed as a system package"),
    }[app.source])
    return chip


def size_text(app: App) -> str:
    return _("About %s") % format_size(app.size_mb)


def pros_cons_box(app: App, spacing: int = 6, max_width_chars: int = -1) -> Gtk.Box:
    box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=spacing)
    for text, icon, css in [(t, "object-select-symbolic", "pro") for t in app.pros_list()] + \
                           [(t, "list-remove-symbolic", "con") for t in app.cons_list()]:
        row = Gtk.Box(spacing=8)
        image = Gtk.Image.new_from_icon_name(icon)
        image.add_css_class(f"{css}-icon")
        image.set_valign(Gtk.Align.START)
        image.set_margin_top(2)
        row.append(image)
        text_label = label(text, f"{css}-text")
        text_label.set_max_width_chars(max_width_chars)
        row.append(text_label)
        box.append(row)
    return box


def clamp(child: Gtk.Widget, width: int = 1100) -> Adw.Clamp:
    widget = Adw.Clamp(maximum_size=width, tightening_threshold=width - 200)
    widget.set_child(child)
    return widget


def scrolled(child: Gtk.Widget) -> Gtk.ScrolledWindow:
    window = Gtk.ScrolledWindow(hscrollbar_policy=Gtk.PolicyType.NEVER, vexpand=True)
    window.set_child(child)
    return window
