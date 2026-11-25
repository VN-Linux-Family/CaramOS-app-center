"""An app's details: what it is, where it comes from, what it is good at, what to know, and its website.

The store shows them as a page with a back button; the first-run picker in a dialog, from a row's info button.
"""

from __future__ import annotations

from gi.repository import Adw, Gtk

from ..i18n import _

from ..catalog import App
from .common import (app_icon, boxed_list, label, open_uri, page_box, page_title, scrolled, secondary, section,
                     source_sentence, themed_image)


def detail_content(app: App, action: Gtk.Widget | None = None) -> Gtk.Box:
    """The details as a page column; action (install or remove) goes at the right of the app's name."""

    box = page_box()
    header = Gtk.Box(spacing=16)
    icon = app_icon(app, 64)
    icon.set_valign(Gtk.Align.START)
    header.append(icon)
    texts = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4, valign=Gtk.Align.CENTER, hexpand=True)
    texts.append(page_title(app.name))
    texts.append(label(app.summary_text(), "paragraph"))
    texts.append(secondary(source_sentence(app)))
    header.append(texts)
    if action is not None:
        action.set_valign(Gtk.Align.CENTER)
        header.append(action)
    box.append(header)
    box.append(section(_("Strengths"), _sentences(app.pros_list())))
    box.append(section(_("Good to know"), _sentences(app.cons_list())))
    if app.website:
        box.append(website_list(app.website))
    return box


def _sentences(items: list[str]) -> Gtk.ListBox:
    listbox = boxed_list()
    for text in items:
        row = Gtk.ListBoxRow(activatable=False)
        row.add_css_class("text-row")
        row.set_child(label(text))
        listbox.append(row)
    return listbox


def display_url(url: str) -> str:
    return url.split("://", 1)[-1].rstrip("/")


def website_list(url: str) -> Gtk.ListBox:
    """A one-row list that opens url in the web browser."""

    listbox = boxed_list()
    row = Gtk.ListBoxRow(activatable=True)
    row.add_css_class("text-row")
    box = Gtk.Box(spacing=12)
    column = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2, hexpand=True)
    column.append(label(_("Website"), "row-title"))
    column.append(secondary(display_url(url), lines=1))
    box.append(column)
    link = themed_image("adw-external-link-symbolic", "external-link-symbolic")
    link.set_valign(Gtk.Align.CENTER)
    box.append(link)
    row.set_child(box)
    row.set_tooltip_text(_("Open %s in the web browser") % display_url(url))
    listbox.append(row)
    listbox.connect("row-activated", lambda widget, _row: open_uri(widget, url))
    return listbox


def detail_dialog(app: App) -> Adw.Dialog:
    """The details in a dialog, for the first-run picker."""

    dialog = Adw.Dialog(title=app.name, content_width=560)
    view = Adw.ToolbarView()
    header = Adw.HeaderBar(show_title=False)
    view.add_top_bar(header)
    content = scrolled(detail_content(app))
    content.set_propagate_natural_height(True)
    view.set_content(content)
    dialog.set_child(view)
    return dialog
