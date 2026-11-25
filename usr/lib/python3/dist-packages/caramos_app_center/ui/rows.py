"""Rows of the App Center's boxed lists.

Every app row has the same shape: the app's icon (40px), its name and one line about it, and the controls at the
right. The app icons are the only colour on the page.
"""

from __future__ import annotations

from collections.abc import Callable

from gi.repository import Gtk

from ..i18n import _

from ..catalog import App
from .common import app_icon, label, secondary, size_text, themed_image

ICON_SIZE = 40


def _text_column(app: App, *, one_line: bool, note: str = "") -> Gtk.Box:
    """Name (600) above the summary (secondary); note, when given, is a third line in the accent colour."""

    column = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2, valign=Gtk.Align.CENTER, hexpand=True)
    column.append(label(app.name, "row-title", lines=1 if one_line else 0))
    column.append(secondary(app.summary_text(), lines=1 if one_line else 0))
    if note:
        column.append(label(note, "secondary", "accent"))
    return column


def _row_box() -> Gtk.Box:
    return Gtk.Box(spacing=12)


def _status_text(text: str) -> Gtk.Label:
    widget = secondary(text, wrap=False, xalign=1.0)
    widget.set_valign(Gtk.Align.CENTER)
    return widget


def _button(text: str, on_clicked: Callable[[], None], *css: str, tooltip: str = "") -> Gtk.Button:
    button = Gtk.Button(label=text, valign=Gtk.Align.CENTER)
    for css_class in css:
        button.add_css_class(css_class)
    if tooltip:
        button.set_tooltip_text(tooltip)
    button.connect("clicked", lambda _button: on_clicked())
    return button


def installed_controls(app: App, on_remove: Callable[[App], None] | None) -> Gtk.Box:
    """Right side of an installed app: "Installed" and Remove, or "Included" for the apps CaramOS comes with."""

    box = Gtk.Box(spacing=12, valign=Gtk.Align.CENTER)
    if app.preinstalled or on_remove is None:
        box.append(_status_text(_("Included") if app.preinstalled else _("Installed")))
        return box
    box.append(_status_text(_("Installed")))
    box.append(_button(_("Remove"), lambda: on_remove(app), tooltip=_("Remove %s from this computer") % app.name))
    return box


class StoreRow(Gtk.ListBoxRow):
    """An app in the store's lists. Activating the row opens the app's page; the button at the right installs or
    removes it."""

    def __init__(self, app: App, installed: bool, *, on_install: Callable[[App], None],
                 on_remove: Callable[[App], None]) -> None:
        super().__init__(activatable=True)
        self.app = app
        self.add_css_class("app-row")
        box = _row_box()
        box.append(app_icon(app, ICON_SIZE))
        box.append(_text_column(app, one_line=True))
        if installed or app.preinstalled:
            box.append(installed_controls(app, on_remove))
        else:
            controls = Gtk.Box(spacing=12, valign=Gtk.Align.CENTER)
            controls.append(_status_text(size_text(app)))
            controls.append(_button(_("Install"), lambda: on_install(app),
                                    tooltip=_("Install %s") % app.name))
            box.append(controls)
        self.set_child(box)
        self.update_property([Gtk.AccessibleProperty.LABEL], [f"{app.name}. {app.summary_text()}"])


class ChoiceRow(Gtk.ListBoxRow):
    """An app to pick in a first-run step: a check box, or a radio button when the step takes one app.
    Activating the row ticks it; the info button shows what the app is good at and what to know."""

    def __init__(self, app: App, *, single: bool, group: Gtk.CheckButton | None, active: bool,
                 on_info: Callable[[App], None], check_sizes: Gtk.SizeGroup) -> None:
        super().__init__(activatable=True)
        self.app = app
        self.single = single
        self.add_css_class("app-row")
        self.check = Gtk.CheckButton(valign=Gtk.Align.CENTER)
        if group is not None:
            self.check.set_group(group)
        self.check.set_active(active)
        self.check.update_property([Gtk.AccessibleProperty.LABEL], [app.name])
        check_sizes.add_widget(self.check)
        box = _row_box()
        box.append(self.check)
        box.append(app_icon(app, ICON_SIZE))
        note = _("Recommended for most people.") if app.badge == "recommended" else ""
        box.append(_text_column(app, one_line=False, note=note))
        box.append(_status_text(size_text(app)))
        info = Gtk.Button(valign=Gtk.Align.CENTER, tooltip_text=_("More about %s") % app.name)
        info.set_child(themed_image("info-outline-symbolic", "dialog-information-symbolic"))
        info.add_css_class("flat")
        info.add_css_class("circular")
        info.add_css_class("row-info")
        info.update_property([Gtk.AccessibleProperty.LABEL], [_("More about %s") % app.name])
        info.connect("clicked", lambda _button: on_info(app))
        box.append(info)
        self.set_child(box)
        self.update_property([Gtk.AccessibleProperty.LABEL], [f"{app.name}. {app.summary_text()}"])

    def activate_choice(self) -> None:
        # A radio button stays on when activated again; a check box flips.
        self.check.set_active(True if self.single else not self.check.get_active())


class InstalledRow(Gtk.ListBoxRow):
    """An installed app in a first-run step: not a choice. The ones the App Center installed can be removed."""

    def __init__(self, app: App, on_remove: Callable[[App], None] | None, *, check_sizes: Gtk.SizeGroup) -> None:
        super().__init__(activatable=False)
        self.app = app
        self.add_css_class("app-row")
        placeholder = Gtk.Box()
        check_sizes.add_widget(placeholder)
        box = _row_box()
        box.append(placeholder)
        box.append(app_icon(app, ICON_SIZE))
        box.append(_text_column(app, one_line=False))
        box.append(installed_controls(app, None if app.preinstalled else on_remove))
        self.set_child(box)
        state = _("Included") if app.preinstalled else _("Installed")
        self.update_property([Gtk.AccessibleProperty.LABEL], [f"{app.name}. {state}"])


class NoneRow(Gtk.ListBoxRow):
    """The "Not now" choice of a step that takes one app or none."""

    def __init__(self, text: str, *, group: Gtk.CheckButton | None, active: bool,
                 check_sizes: Gtk.SizeGroup) -> None:
        super().__init__(activatable=True)
        self.app = None
        self.add_css_class("app-row")
        self.check = Gtk.CheckButton(valign=Gtk.Align.CENTER)
        if group is not None:
            self.check.set_group(group)
        self.check.set_active(active)
        self.check.update_property([Gtk.AccessibleProperty.LABEL], [text])
        check_sizes.add_widget(self.check)
        box = _row_box()
        box.append(self.check)
        # No icon: the text lines up with the app names above it.
        box.append(Gtk.Box(width_request=ICON_SIZE))
        text_label = label(text)
        text_label.set_valign(Gtk.Align.CENTER)
        text_label.set_hexpand(True)
        box.append(text_label)
        self.set_child(box)

    def activate_choice(self) -> None:
        self.check.set_active(True)


class ProgressRow(Gtk.ListBoxRow):
    """An app being installed or removed: its state in words, with a progress bar or a spinner at the right."""

    def __init__(self, app: App, state: str) -> None:
        super().__init__(activatable=False)
        self.app = app
        self.add_css_class("app-row")
        self.fraction = 0.0
        box = _row_box()
        box.append(app_icon(app, ICON_SIZE))
        column = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2, valign=Gtk.Align.CENTER, hexpand=True)
        column.append(label(app.name, "row-title", lines=1))
        self.state = secondary(state, lines=1)
        column.append(self.state)
        box.append(column)
        self.bar = Gtk.ProgressBar(valign=Gtk.Align.CENTER, width_request=120)
        self.spinner = Gtk.Spinner(valign=Gtk.Align.CENTER, width_request=16, height_request=16)
        self.side = Gtk.Stack(valign=Gtk.Align.CENTER, hhomogeneous=False, vhomogeneous=False)
        self.side.add_named(Gtk.Box(), "none")
        self.side.add_named(self.spinner, "spinner")
        self.side.add_named(self.bar, "bar")
        self.side.set_visible_child_name("none")
        box.append(self.side)
        self.set_child(box)

    def set_state(self, text: str, *, error: bool = False) -> None:
        self.state.set_label(text)
        if error:
            self.state.remove_css_class("dim-label")
            self.state.add_css_class("error")

    def busy(self, text: str) -> None:
        self.set_state(text)
        self.spinner.start()
        self.side.set_visible_child_name("spinner")

    def progress(self, fraction: float, text: str) -> None:
        self.fraction = fraction
        self.bar.set_fraction(fraction)
        self.set_state(text)
        self.spinner.stop()
        self.side.set_visible_child_name("bar")

    def finish(self, ok: bool, text: str) -> None:
        if ok:
            self.fraction = 1.0
        self.spinner.stop()
        self.side.set_visible_child_name("none")
        self.set_state(text, error=not ok)
