"""Main window: loading, offline, app picker, install (or remove) progress and done page.

The store list (StoreView) is hidden for now and only opens with --store: it should list apps from the
VNLF app site API, which is not ready. Everything else, including the Desktop and menu launchers, opens
the picker.
"""

from __future__ import annotations

import threading
from datetime import datetime

from gi.repository import Adw, Gio, GLib, Gtk

from ..i18n import _, ngettext

from .. import firstrun, problem
from ..catalog import App, Catalog
from ..detect import installed_apps
from .common import app_icon, boxed_list, clamp, label, secondary
from .install import InstallView, Results
from .store import StoreView
from .wizard import WizardView


class MainWindow(Adw.ApplicationWindow):
    def __init__(self, application: Adw.Application, catalog: Catalog, *, store: bool, first_run: bool) -> None:
        super().__init__(application=application, title=_("CaramOS App Center"), default_width=1080,
                         default_height=760)
        self.set_size_request(360, 480)
        self.catalog = catalog
        self.want_store = store
        self.first_run = first_run
        self.installed: set[str] = set()
        self.install_view: InstallView | None = None
        self.store: StoreView | None = None
        self.wizard: WizardView | None = None
        self.hidden_install = False
        self.running_action = "install"
        # The last install or removal, for asking VNLF for help when an app failed (problem.py).
        self.last_apps: list[App] = []
        self.last_logs: dict[str, list[str]] = {}
        self.last_reasons: dict[str, str] = {}
        self.last_finished: datetime | None = None
        self.reopen_hint_shown = False

        self.stack = Gtk.Stack(transition_type=Gtk.StackTransitionType.CROSSFADE, transition_duration=220)
        self.toasts = Adw.ToastOverlay(child=self.stack)
        self.set_content(self.toasts)

        action = Gio.SimpleAction.new("run-wizard", None)
        action.connect("activate", lambda *_a: self.show_wizard())
        self.add_action(action)
        self.connect("close-request", self._on_close_request)

        self.network = Gio.NetworkMonitor.get_default()
        self._show_loading()
        self._detect_installed()

    # Pages -----------------------------------------------------------------------------------

    def _set_page(self, name: str, widget: Gtk.Widget) -> None:
        old = self.stack.get_child_by_name(name)
        if old is not None:
            self.stack.remove(old)
        self.stack.add_named(widget, name)
        self.stack.set_visible_child_name(name)

    @staticmethod
    def _with_header(content: Gtk.Widget, title: str = "") -> Adw.ToolbarView:
        view = Adw.ToolbarView()
        header = Adw.HeaderBar()
        if title:
            header.set_title_widget(Adw.WindowTitle(title=title))
        view.add_top_bar(header)
        view.set_content(content)
        return view

    def _show_loading(self) -> None:
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=16, valign=Gtk.Align.CENTER)
        spinner = Gtk.Spinner(spinning=True, width_request=32, height_request=32)
        box.append(spinner)
        box.append(label(_("Checking installed apps…"), "dim-label", xalign=0.5))
        self._set_page("loading", self._with_header(box))

    def _detect_installed(self) -> None:
        def work() -> None:
            installed = installed_apps(self.catalog)
            GLib.idle_add(self._on_detected, installed)

        threading.Thread(target=work, daemon=True).start()

    def _on_detected(self, installed: set[str]) -> bool:
        self.installed = installed
        if self.want_store:
            self.show_store()
        else:
            self.show_wizard()
        return False

    def _online(self) -> bool:
        return self.network.get_network_available() and \
            self.network.get_connectivity() in (Gio.NetworkConnectivity.FULL, Gio.NetworkConnectivity.LIMITED)

    def show_wizard(self, *, keep_choices: bool = False, selection: dict[str, set[str]] | None = None) -> None:
        if not self._online():
            self._show_offline()
            return
        if keep_choices and self.wizard is not None:
            self.stack.set_visible_child_name("wizard")
            return
        self.wizard = WizardView(self.catalog, self.installed, on_install=self._install_from_wizard,
                                 on_remove=self._remove_from_wizard, on_finish=self._finish_wizard,
                                 on_later=self.close if self.first_run else None, selection=selection)
        self._set_page("wizard", self.wizard)

    def _show_offline(self) -> None:
        if self.first_run:
            description = _("Connect to Wi-Fi or a network cable to download apps. CaramOS asks again at the next "
                            "login.")
        else:
            description = _("Connect to Wi-Fi or a network cable to download apps.")
        page = Adw.StatusPage(icon_name="network-offline-symbolic", title=_("No Internet connection"),
                              description=description)
        buttons = Gtk.Box(spacing=12, halign=Gtk.Align.CENTER)
        buttons.append(_pill(_("Later"), self.close))
        buttons.append(_pill(_("Try again"), self.show_wizard, suggested=True))
        page.set_child(buttons)
        self._set_page("offline", self._with_header(page))

        def on_changed(*_args) -> None:
            if self._online() and self.stack.get_visible_child_name() == "offline":
                self.show_wizard()

        self.network.connect("network-changed", on_changed)

    def show_store(self) -> None:
        if self.store is None:
            self.store = StoreView(self.catalog, self.installed, on_action=self._store_action)
            self.stack.add_named(self.store, "store")
        else:
            self.store.set_installed(self.installed)
        self.stack.set_visible_child_name("store")

    # Install flow ----------------------------------------------------------------------------

    def _start_install(self, apps: list[App], action: str, after) -> None:
        def cancelled() -> None:
            self.install_view = None
            self.toasts.add_toast(Adw.Toast(title=_("Authentication was cancelled. Nothing was changed.")))
            after(None)

        def done(results: Results) -> None:
            if self.install_view is not None:
                self.last_apps = list(apps)
                self.last_logs = {app_id: list(lines) for app_id, lines in self.install_view.logs.items()}
                self.last_reasons = dict(self.install_view.reasons)
                self.last_finished = datetime.now()
            self.install_view = None
            self._detect_after(results, after)

        self.running_action = action
        self.install_view = InstallView(apps, action=action, on_done=done, on_cancelled=cancelled)
        self._set_page("install", self.install_view)
        self.install_view.start()

    def _detect_after(self, results: Results, after) -> None:
        def work() -> None:
            installed = installed_apps(self.catalog)
            GLib.idle_add(finish, installed)

        def finish(installed: set[str]) -> bool:
            self.installed = installed
            if self.hidden_install:
                self._notify_and_quit(results)
            else:
                after(results)
            return False

        threading.Thread(target=work, daemon=True).start()

    def _install_from_wizard(self, ids: list[str]) -> None:
        apps = [self.catalog.app(app_id) for app_id in ids]

        def after(results: Results | None) -> None:
            if results is None:
                # Authentication cancelled: back to the summary with the same choices.
                self.show_wizard(keep_choices=True)
            else:
                self._show_done(results)

        self._start_install(apps, "install", after)

    def _confirm_remove(self, app: App, remove) -> None:
        """Ask before removing an app; remove() runs when the user agrees."""

        dialog = Adw.AlertDialog(heading=_("Remove %s?") % app.name,
                                 body=_("It is removed from this computer for every user. Your documents and other "
                                        "files stay."))
        dialog.add_response("cancel", _("Cancel"))
        dialog.add_response("remove", _("Remove"))
        dialog.set_response_appearance("remove", Adw.ResponseAppearance.DESTRUCTIVE)
        dialog.set_default_response("cancel")
        dialog.set_close_response("cancel")
        dialog.connect("response", lambda _dialog, response: remove() if response == "remove" else None)
        dialog.present(self)

    def _remove_from_wizard(self, app: App) -> None:
        def remove() -> None:
            selection = self.wizard.selection if self.wizard is not None else None

            def after(results: Results | None) -> None:
                self.show_wizard(selection=selection)
                if results is not None:
                    self.toasts.add_toast(self._result_toast(app, "remove", results))

            self._start_install([app], "remove", after)

        self._confirm_remove(app, remove)

    def _finish_wizard(self) -> None:
        if self.first_run:
            firstrun.mark_done()
        self.reopen_hint_shown = True
        self.close()

    def _show_done(self, results: Results) -> None:
        if self.first_run:
            firstrun.mark_done()
        failed = [self.catalog.app(app_id) for app_id, (ok, _error) in results.items() if not ok]
        done = len(results) - len(failed)
        content = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=24)
        buttons = Gtk.Box(spacing=12, halign=Gtk.Align.CENTER)
        if failed:
            if len(failed) == 1:
                title = _("%s was not installed") % failed[0].name
            else:
                title = ngettext("%d app was not installed", "%d apps were not installed", len(failed)) % len(failed)
            sentences = []
            if done:
                sentences.append(ngettext("%d other app is installed.", "%d other apps are installed.", done) % done)
            if problem.help_available():
                sentences.append(_("Try again now or later. If it still fails, press Ask for help to send VNLF what "
                                   "the installer logged."))
            else:
                sentences.append(_("Try again now or later. If it still fails, send the details below from "
                                   "Feedback & Help."))
            # The description is Pango markup: "&" in "Feedback & Help" would hide it.
            page = Adw.StatusPage(icon_name="dialog-warning-symbolic", title=title,
                                  description=GLib.markup_escape_text(" ".join(sentences)))
            content.append(clamp(self._failed_list(failed, results), 480))
            if problem.help_available():
                buttons.append(_pill(_("Ask for help"), lambda: self._ask_for_help("install", results)))
            buttons.append(_pill(_("Try again"), lambda: self._install_from_wizard([app.id for app in failed])))
        else:
            page = Adw.StatusPage(icon_name="emblem-ok-symbolic",
                                  title=ngettext("Installed %d app", "Installed %d apps", done) % done,
                                  description=ngettext("You can find it in the Start menu.",
                                                       "You can find them in the Start menu.", done))
            # Nothing ticked: the recommended apps were ticked once, at the first run, and are not wanted again.
            buttons.append(_pill(_("Choose more apps"), lambda: self.show_wizard(selection={})))
        buttons.append(_pill(_("Close"), self.close, suggested=True))
        content.append(buttons)
        page.set_child(content)
        self._set_page("done", self._with_header(page))

    def _failed_list(self, apps: list[App], results: Results) -> Gtk.ListBox:
        """The apps that were not installed, with what to do when the App Center knows, and the installer's own
        words under Details."""

        listbox = boxed_list()
        for app in apps:
            row = Gtk.ListBoxRow(activatable=False)
            row.add_css_class("app-row")
            box = Gtk.Box(spacing=12)
            box.append(app_icon(app, 40))
            column = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2, valign=Gtk.Align.CENTER, hexpand=True)
            column.append(label(app.name, "row-title"))
            advice = results.get(app.id, (False, ""))[1]
            if advice and advice != self.last_reasons.get(app.id, advice):
                column.append(secondary(advice))
            box.append(column)
            row.set_child(box)
            listbox.append(row)
        reasons = "\n".join(f"{app.name}: {self.last_reasons.get(app.id) or results.get(app.id, (False, ''))[1]}"
                             for app in apps)
        details = Adw.ExpanderRow(title=_("Details"))
        text = label(reasons, "monospace", "details-text")
        text.set_selectable(True)
        inner = Gtk.ListBoxRow(activatable=False)
        inner.add_css_class("text-row")
        inner.set_child(text)
        details.add_row(inner)
        listbox.append(details)
        return listbox

    def _store_action(self, app: App, action: str) -> None:
        def after(results: Results | None) -> None:
            self.show_store()
            if results is not None:
                self.toasts.add_toast(self._result_toast(app, action, results))

        if action == "remove":
            self._confirm_remove(app, lambda: self._start_install([app], action, after))
        else:
            self._start_install([app], action, after)

    def _result_toast(self, app: App, action: str, results: Results) -> Adw.Toast:
        toast = Adw.Toast(title=_change_text(app, action, results), timeout=6)
        if not results.get(app.id, (False, ""))[0] and problem.help_available():
            toast.set_button_label(_("Ask for help"))
            toast.set_timeout(0)   # the user may want the button: it stays until dismissed
            toast.connect("button-clicked", lambda _toast: self._ask_for_help(action, results))
        return toast

    def _ask_for_help(self, action: str, results: Results) -> None:
        names = ", ".join(app.name for app in problem.failed(self.last_apps, results))
        if action == "remove":
            message = _("The App Center could not remove %s.") % names
        else:
            message = _("The App Center could not install %s.") % names
        if not problem.ask_for_help(action, self.last_apps, results, self.last_logs, message, self.last_finished):
            self.toasts.add_toast(Adw.Toast(title=_("Feedback & Help could not be opened."), timeout=6))

    # Closing while installing ----------------------------------------------------------------

    def _on_close_request(self, _window) -> bool:
        if self.install_view is not None and self.install_view.running:
            # Keep installing in the background; the application stays alive until it finishes.
            self.hidden_install = True
            self.get_application().hold()
            self.set_visible(False)
            return True
        # Only the autostarted picker explains where to find it again; a user who opened it knows.
        if self.first_run and not self.reopen_hint_shown and \
                self.stack.get_visible_child_name() in ("wizard", "offline"):
            self._show_reopen_hint()
            return True
        return False

    def _show_reopen_hint(self) -> None:
        """Closing the picker without installing: tell where to find the App Center again."""

        self.reopen_hint_shown = True
        dialog = Adw.AlertDialog(heading=_("Open the App Center again any time"),
                                 body=_("Double-click App Center on the Desktop, or look for “App Center” in the "
                                        "Start menu."))
        dialog.add_response("ok", _("Got it"))
        dialog.set_default_response("ok")
        dialog.set_close_response("ok")
        dialog.connect("response", lambda *_args: self.close())
        dialog.present(self)

    def _notify_and_quit(self, results: Results) -> None:
        if self.first_run:
            firstrun.mark_done()
        self.hidden_install = False
        application = self.get_application()
        failed = sum(1 for ok, _err in results.values() if not ok)
        notification = Gio.Notification.new(_("CaramOS App Center"))
        if self.running_action == "remove":
            app = self.catalog.app(next(iter(results)))
            notification.set_body(_change_text(app, "remove", results))
        elif failed:
            notification.set_body(ngettext("%d app could not be installed.", "%d apps could not be installed.",
                                           failed) % failed)
        else:
            notification.set_body(_("Your apps are installed and ready in the menu."))
        notification.set_icon(Gio.ThemedIcon.new("caramos-logo"))
        application.send_notification("installed", notification)
        application.release()
        self.close()


def _change_text(app: App, action: str, results: Results) -> str:
    ok, error = results.get(app.id, (False, ""))
    if ok:
        return _("%s is installed") % app.name if action == "install" else _("%s was removed") % app.name
    values = {"app": app.name, "error": error or _("unknown error")}
    if action == "install":
        return _("Could not install %(app)s: %(error)s") % values
    return _("Could not remove %(app)s: %(error)s") % values


def _pill(text: str, on_clicked, *, suggested: bool = False) -> Gtk.Button:
    """A button of a status page (the done and offline pages)."""

    button = Gtk.Button(label=text)
    button.add_css_class("pill")
    if suggested:
        button.add_css_class("suggested-action")
    button.connect("clicked", lambda _button: on_clicked())
    return button
