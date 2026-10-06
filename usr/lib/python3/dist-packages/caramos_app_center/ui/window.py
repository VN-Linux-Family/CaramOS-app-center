"""Main window: loading, offline, first-run wizard, install progress, done page and store."""

from __future__ import annotations

import threading

from gi.repository import Adw, Gdk, Gio, GLib, Gtk

from ..i18n import _, ngettext

from .. import firstrun
from ..catalog import App, Catalog
from ..detect import installed_apps
from .common import label, logo_path
from .install import InstallView, Results
from .store import StoreView
from .wizard import WizardView


class MainWindow(Adw.ApplicationWindow):
    def __init__(self, application: Adw.Application, catalog: Catalog, *, wizard: bool, first_run: bool) -> None:
        super().__init__(application=application, title=_("CaramOS App Center"), default_width=1080,
                         default_height=760)
        self.set_size_request(360, 480)
        self.catalog = catalog
        self.want_wizard = wizard
        self.first_run = first_run
        self.installed: set[str] = set()
        self.install_view: InstallView | None = None
        self.store: StoreView | None = None
        self.wizard: WizardView | None = None
        self.hidden_install = False

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
        if self.want_wizard:
            self.show_wizard()
        else:
            self.show_store()
        return False

    def _online(self) -> bool:
        return self.network.get_network_available() and \
            self.network.get_connectivity() in (Gio.NetworkConnectivity.FULL, Gio.NetworkConnectivity.LIMITED)

    def show_wizard(self, *, keep_choices: bool = False) -> None:
        if not self._online():
            self._show_offline()
            return
        if keep_choices and self.wizard is not None:
            self.stack.set_visible_child_name("wizard")
            return
        self.wizard = WizardView(self.catalog, self.installed, on_install=self._install_from_wizard,
                                 on_finish=self._finish_wizard, on_later=self.close)
        self._set_page("wizard", self.wizard)

    def _show_offline(self) -> None:
        page = Adw.StatusPage(icon_name="network-offline-symbolic", title=_("No Internet connection"),
                              description=_("Connect to Wi-Fi or a network cable to download apps. "
                                            "CaramOS asks again at the next login."))
        buttons = Gtk.Box(spacing=12, halign=Gtk.Align.CENTER)
        retry = Gtk.Button(label=_("Try again"))
        retry.add_css_class("suggested-action")
        retry.add_css_class("pill")
        retry.connect("clicked", lambda _b: self.show_wizard())
        later = Gtk.Button(label=_("Later"))
        later.add_css_class("pill")
        later.connect("clicked", lambda _b: self.close())
        buttons.append(retry)
        buttons.append(later)
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
            self.install_view = None
            self._detect_after(results, after)

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

    def _finish_wizard(self) -> None:
        if self.first_run:
            firstrun.mark_done()
        self.show_store()

    def _show_done(self, results: Results) -> None:
        if self.first_run:
            firstrun.mark_done()
        failed = [self.catalog.app(app_id).name for app_id, (ok, _err) in results.items() if not ok]
        done = len(results) - len(failed)
        if failed:
            page = Adw.StatusPage(icon_name="dialog-warning-symbolic", title=_("Some apps were not installed"),
                                  description=_("Not installed: %s. Check the connection and try again from the "
                                                "App Center.") % ", ".join(failed))
        else:
            page = Adw.StatusPage(title=_("CaramOS is ready"),
                                  description=ngettext("%d app was installed. You can find it in the menu.",
                                                       "%d apps were installed. You can find them in the menu.",
                                                       done) % done)
            if logo_path().is_file():
                page.set_paintable(Gdk.Texture.new_from_filename(str(logo_path())))
            else:
                page.set_icon_name("emblem-ok-symbolic")
        buttons = Gtk.Box(spacing=12, halign=Gtk.Align.CENTER)
        store = Gtk.Button(label=_("Open the App Center"))
        store.add_css_class("pill")
        store.connect("clicked", lambda _b: self.show_store())
        close = Gtk.Button(label=_("Done"))
        close.add_css_class("suggested-action")
        close.add_css_class("pill")
        close.connect("clicked", lambda _b: self.close())
        buttons.append(store)
        buttons.append(close)
        page.set_child(buttons)
        self._set_page("done", self._with_header(page))

    def _store_action(self, app: App, action: str) -> None:
        def after(results: Results | None) -> None:
            self.show_store()
            if results is None:
                return
            ok, error = results.get(app.id, (False, ""))
            if ok:
                text = _("%s is installed") % app.name if action == "install" else _("%s was removed") % app.name
            else:
                text = _("Could not change %(app)s: %(error)s") % {"app": app.name, "error": error or _("unknown error")}
            self.toasts.add_toast(Adw.Toast(title=text, timeout=6))

        self._start_install([app], action, after)

    # Closing while installing ----------------------------------------------------------------

    def _on_close_request(self, _window) -> bool:
        if self.install_view is not None and self.install_view.running:
            # Keep installing in the background; the application stays alive until it finishes.
            self.hidden_install = True
            self.get_application().hold()
            self.set_visible(False)
            return True
        return False

    def _notify_and_quit(self, results: Results) -> None:
        if self.first_run:
            firstrun.mark_done()
        self.hidden_install = False
        application = self.get_application()
        failed = sum(1 for ok, _err in results.values() if not ok)
        notification = Gio.Notification.new(_("CaramOS App Center"))
        if failed:
            notification.set_body(ngettext("%d app could not be installed.", "%d apps could not be installed.",
                                           failed) % failed)
        else:
            notification.set_body(_("Your apps are installed and ready in the menu."))
        notification.set_icon(Gio.ThemedIcon.new("caramos-logo"))
        application.send_notification("installed", notification)
        application.release()
        self.close()
