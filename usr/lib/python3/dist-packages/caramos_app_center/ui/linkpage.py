"""The pages an install link of the VNLF app site opens (caramos-app://install/<Flathub app id>, links.py).

An app the App Center suggests opens on its own card, with its Install or Remove button. Any other Flathub app opens on a
page that says what Flathub says about it (developer, licence, size) and that it comes from Flathub, with Install and
Later at the bottom, as the first-run picker has its buttons.
"""

from __future__ import annotations

from collections.abc import Callable

from gi.repository import Adw, Gtk

from ..i18n import _

from .. import flatpak_ops
from ..catalog import App
from ..flathub import FlathubApp
from ..plan import format_size
from .common import app_icon, boxed_list, clamp, label, page_box, page_title, scrolled, secondary, section
from .detail import detail_content, website_list
from .rows import installed_controls

MB = 1000 * 1000


def setup_note(state: str) -> str:
    """What the App Center does before the app, when Flatpak or Flathub is missing (flatpak_ops.flathub_state)."""

    if state == flatpak_ops.NO_FLATPAK:
        return _("Flatpak is not on this computer. The App Center will install Flatpak first, then the app.")
    if state in (flatpak_ops.NO_REMOTE, flatpak_ops.REMOTE_DISABLED):
        return _("This computer does not use Flathub yet. The App Center will turn Flathub on first, then install the "
                 "app.")
    return ""


def licence_text(info: FlathubApp) -> str:
    if info.free is True and info.licence:
        return _("Open source (%s).") % info.licence
    if info.free is False:
        return _("Closed source, under its developer's terms.")
    return _("Flathub does not say.")


def size_text(info: FlathubApp, runtime_missing: bool) -> str:
    sentences = []
    if info.download_size or info.installed_size:
        sentences.append(_("About %(download)s to download, about %(installed)s once installed.") % {
            "download": format_size(max(1, round(info.download_size / MB))),
            "installed": format_size(max(1, round(info.installed_size / MB)))})
    if runtime_missing:
        sentences.append(_("The first time, it also downloads a runtime that the apps from Flathub share, which can "
                           "take more than 1 GB."))
    return " ".join(sentences) or _("Flathub does not say.")


def _info_row(title: str, text: str) -> Gtk.ListBoxRow:
    row = Gtk.ListBoxRow(activatable=False)
    row.add_css_class("text-row")
    column = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
    column.append(label(title, "row-title"))
    column.append(secondary(text))
    row.set_child(column)
    return row


def _header(on_back: Callable[[], None] | None) -> Adw.HeaderBar:
    header = Adw.HeaderBar(show_title=False)
    if on_back is not None:
        back = Gtk.Button(icon_name="go-previous-symbolic", tooltip_text=_("Back"))
        back.update_property([Gtk.AccessibleProperty.LABEL], [_("Back")])
        back.connect("clicked", lambda _button: on_back())
        header.pack_start(back)
    return header


def _button(text: str, on_clicked: Callable[[], None], *css: str) -> Gtk.Button:
    button = Gtk.Button(label=text, valign=Gtk.Align.CENTER)
    for css_class in css:
        button.add_css_class(css_class)
    button.connect("clicked", lambda _button: on_clicked())
    return button


def catalog_page(app: App, *, installed: bool, setup: str, on_install: Callable[[App], None],
                 on_remove: Callable[[App], None], on_back: Callable[[], None] | None) -> Gtk.Widget:
    """An app the App Center suggests: its own card, as the store shows it, with Install, or Installed and Remove."""

    if app.preinstalled:
        action = None
    elif installed:
        action = installed_controls(app, on_remove)
    else:
        action = _button(_("Install"), lambda: on_install(app), "suggested-action")
    column = detail_content(app, action)
    note = setup_note(setup) if app.method == "flatpak" and not installed else ""
    if note:
        column.append(secondary(note, "paragraph"))
    view = Adw.ToolbarView()
    view.add_top_bar(_header(on_back))
    view.set_content(scrolled(clamp(column)))
    return view


def flathub_page(info: FlathubApp, app: App, *, installed: bool, setup: str, runtime_missing: bool,
                 on_install: Callable[[App], None], on_remove: Callable[[App], None], on_later: Callable[[], None],
                 on_back: Callable[[], None] | None) -> Gtk.Widget:
    """A Flathub app the App Center does not suggest: what Flathub says about it, and Install / Later."""

    column = page_box()
    head = Gtk.Box(spacing=16)
    icon = app_icon(app, 64)
    icon.set_valign(Gtk.Align.START)
    head.append(icon)
    texts = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4, valign=Gtk.Align.CENTER, hexpand=True)
    texts.append(page_title(info.name))
    if info.summary:
        texts.append(label(info.summary, "paragraph"))
    head.append(texts)
    column.append(head)

    facts = boxed_list()
    developer = info.developer or _("Flathub does not say.")
    if info.verified:
        developer = _("%s. Verified by Flathub.") % developer.rstrip(".")
    facts.append(_info_row(_("Developer"), developer))
    facts.append(_info_row(_("Licence"), licence_text(info)))
    facts.append(_info_row(_("Size"), size_text(info, runtime_missing and not installed)))
    column.append(section(_("About this app"), facts))

    about = [_("Flathub is the shared app store for Linux. This app is not on the App Center's list of suggestions, "
               "so the CaramOS team has not tried it on CaramOS.")]
    if info.end_of_life:
        about.append(_("Its developer no longer updates this app."))
    note = setup_note(setup) if not installed else ""
    if note:
        about.append(note)
    flathub_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
    for sentence in about:
        flathub_box.append(label(sentence, "paragraph"))
    heading = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
    heading.append(label(_("An app from Flathub"), "section-title", heading=True))
    heading.append(flathub_box)
    column.append(heading)
    if info.homepage:
        column.append(website_list(info.homepage))

    bar = Gtk.Box(spacing=12)
    bar.add_css_class("action-bar")
    if installed:
        hint = label(_("Installed on this computer."), "row-title", lines=1)
        hint.set_hexpand(True)
        bar.append(hint)
        bar.append(_button(_("Remove"), lambda: on_remove(app)))
        bar.append(_button(_("Close"), on_later, "suggested-action"))
    else:
        hint = secondary(_("From Flathub, about %s.") % format_size(app.size_mb), lines=1)
        hint.set_hexpand(True)
        bar.append(hint)
        bar.append(_button(_("Later"), on_later))
        bar.append(_button(_("Install"), lambda: on_install(app), "suggested-action"))

    view = Adw.ToolbarView(bottom_bar_style=Adw.ToolbarStyle.RAISED_BORDER)
    view.add_top_bar(_header(on_back))
    view.set_content(scrolled(clamp(column)))
    view.add_bottom_bar(clamp(bar))
    return view


def loading_page() -> Gtk.Widget:
    box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=16, valign=Gtk.Align.CENTER)
    box.append(Gtk.Spinner(spinning=True, width_request=32, height_request=32))
    box.append(label(_("Getting the app's details from Flathub…"), "dim-label", xalign=0.5))
    view = Adw.ToolbarView()
    view.add_top_bar(Adw.HeaderBar(show_title=False))
    view.set_content(box)
    return view
