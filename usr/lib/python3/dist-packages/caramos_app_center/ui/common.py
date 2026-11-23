"""Small widget helpers shared by the App Center pages.

The look follows CaramOS's design rules (DESIGN.md in the CaramOS repository): libadwaita's own header bars,
surfaces and boxed lists, the forest green accent as the only colour, two font weights, and words instead of
chips: where an app comes from and how big it is reads as a sentence.
"""

from __future__ import annotations

from html import escape
from pathlib import Path

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")
from gi.repository import Adw, Gdk, Gio, Gtk, Pango  # noqa: E402

from ..i18n import _  # noqa: E402

from ..catalog import App, data_dir, more_apps_url  # noqa: E402
from ..plan import format_size  # noqa: E402

SYSTEM_LOGO = Path("/usr/share/pixmaps/caramos-logo.png")
MORE_APPS_SITE = "apps.vietnamlinuxfamily.net"
# Content width of the pages: rows stay readable on large screens.
CONTENT_WIDTH = 720


def open_uri(parent: Gtk.Widget, uri: str) -> None:
    root = parent.get_root()
    Gtk.UriLauncher.new(uri).launch(root if isinstance(root, Gtk.Window) else None, None, None, None)


def open_more_apps(parent: Gtk.Widget, query: str = "", category: str = "") -> None:
    """Open the VNLF app site in the default browser, searching for query or showing a category."""

    open_uri(parent, more_apps_url(query, category))


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


def label(text: str, *classes: str, wrap: bool = True, xalign: float = 0.0, lines: int = 0,
          heading: bool = False) -> Gtk.Label:
    """A left-aligned label; lines=1 keeps it on one line and ends it with "…" when it does not fit. heading: what
    screen readers announce as a heading."""

    if heading:
        widget = Gtk.Label(label=text, xalign=xalign, accessible_role=Gtk.AccessibleRole.HEADING)
    else:
        widget = Gtk.Label(label=text, xalign=xalign)
    if lines:
        widget.set_lines(lines)
        widget.set_ellipsize(Pango.EllipsizeMode.END)
        widget.set_wrap(lines > 1)
        if lines == 1:
            widget.set_single_line_mode(True)
    else:
        widget.set_wrap(wrap)
    widget.set_wrap_mode(Pango.WrapMode.WORD_CHAR)
    for css_class in classes:
        widget.add_css_class(css_class)
    return widget


def secondary(text: str, *classes: str, wrap: bool = True, xalign: float = 0.0, lines: int = 0) -> Gtk.Label:
    """Secondary text: smaller and in the secondary text colour."""

    return label(text, "dim-label", "secondary", *classes, wrap=wrap, xalign=xalign, lines=lines)


def link_label(text: str, link_text: str, uri: str, *classes: str, tooltip: str = "") -> Gtk.Label:
    """A sentence followed by a link; the link opens in the web browser."""

    markup = escape(text)
    if text:
        markup += " "
    title = f' title="{escape(tooltip)}"' if tooltip else ""
    markup += f'<a href="{escape(uri)}"{title}>{escape(link_text)}</a>'
    widget = label("", *classes)
    widget.set_markup(markup)

    def activate(widget_: Gtk.Label, target: str) -> bool:
        open_uri(widget_, target)
        return True

    widget.connect("activate-link", activate)
    return widget


def themed_image(*names: str, size: int = 16) -> Gtk.Image:
    """An icon from the icon theme, trying names in order (icon themes do not all have the newer names)."""

    image = Gtk.Image.new_from_gicon(Gio.ThemedIcon.new_from_names(list(names)))
    image.set_pixel_size(size)
    return image


def app_icon(app: App, size: int) -> Gtk.Image:
    path = app.icon_path(data_dir() / "icons")
    image = Gtk.Image.new_from_file(str(path)) if path and path.is_file() else Gtk.Image.new_from_icon_name(
        app.icon_name or "application-x-executable")
    image.set_pixel_size(size)
    image.set_valign(Gtk.Align.CENTER)
    image.add_css_class("app-icon")
    return image


def logo(size: int) -> Gtk.Widget:
    path = logo_path()
    if path.is_file():
        image = Gtk.Image.new_from_file(str(path))
    else:
        image = Gtk.Image.new_from_icon_name("system-software-install")
    image.set_pixel_size(size)
    return image


def source_sentence(app: App) -> str:
    """Where the app comes from and how much it takes, as one sentence: "From Flathub, about 1.3 GB." """

    size = format_size(app.size_mb)
    return {
        "flathub": _("From Flathub, about %s.") % size,
        "ubuntu": _("From the Ubuntu software archive, about %s.") % size,
        "caramos": _("Comes with CaramOS."),
        "caramos-package": _("From the CaramOS software archive, about %s.") % size,
        "community": _("A Linux version made by the community, about %s.") % size,
        "developer": _("From the developer's website, about %s.") % size,
    }[app.source]


def page_title(text: str) -> Gtk.Label:
    return label(text, "page-title", heading=True)


def section_title(text: str) -> Gtk.Label:
    return label(text, "section-title", heading=True)


def boxed_list() -> Gtk.ListBox:
    listbox = Gtk.ListBox(selection_mode=Gtk.SelectionMode.NONE)
    listbox.add_css_class("boxed-list")
    return listbox


def section(title: str, listbox: Gtk.Widget, *, hint: str = "", description: str | Gtk.Widget = "") -> Gtk.Box:
    """A titled group: the title (with a hint at the right), an optional description, then the list 8px below."""

    box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
    head = Gtk.Box(spacing=12)
    heading = section_title(title)
    heading.set_hexpand(True)
    head.append(heading)
    if hint:
        hint_label = secondary(hint, wrap=False, xalign=1.0)
        hint_label.set_valign(Gtk.Align.BASELINE_FILL)
        heading.set_valign(Gtk.Align.BASELINE_FILL)
        head.append(hint_label)
    box.append(head)
    if description:
        text = secondary(description) if isinstance(description, str) else description
        text.set_margin_top(2)
        box.append(text)
    listbox.set_margin_top(8)
    box.append(listbox)
    return box


def page_box() -> Gtk.Box:
    """The column of a page: 24px from the window edges, 24px between sections."""

    box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=24)
    box.set_margin_top(24)
    box.set_margin_bottom(32)
    box.set_margin_start(24)
    box.set_margin_end(24)
    return box


def clamp(child: Gtk.Widget, width: int = CONTENT_WIDTH) -> Adw.Clamp:
    widget = Adw.Clamp(maximum_size=width, tightening_threshold=width)
    widget.set_child(child)
    return widget


def scrolled(child: Gtk.Widget) -> Gtk.ScrolledWindow:
    window = Gtk.ScrolledWindow(hscrollbar_policy=Gtk.PolicyType.NEVER, vexpand=True)
    window.set_child(child)
    return window
