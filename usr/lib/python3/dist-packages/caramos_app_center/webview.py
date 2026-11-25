"""The window of a website app: caramos-web-app <app-id> shows that one site, without a browser around it.

GTK 3 and WebKitGTK 4.1, which both CaramOS editions ship. What the window lets through is in webapp.py: links to other
sites open in the default browser, pop-ups are dropped, nothing is downloaded, no camera, microphone, location or
notifications, no developer tools, and the ad and tracker servers are blocked where the catalog says so. The page runs in
WebKit's web process like in any browser; the window adds no code to it. Each site keeps its cookies and storage in its
own folder (~/.local/share/caramos-web-app/<id>), apart from the browser and from the other sites.
"""

from __future__ import annotations

import hashlib
import json
import os
import sys
from pathlib import Path

from . import renderer, webapp
from .catalog import ID_RE, App, CatalogError, data_dir, load_catalog
from .i18n import _

FILTER_PREFIX = "caramos-ads-"
# The one coloured button (Try again) in CaramOS's forest green (DESIGN.md: GTK 3 programs use its values).
CSS = b"""
button.suggested-action { background-image: none; background-color: #2b6a44; color: #ffffff; border-color: #2b6a44; }
button.suggested-action:hover { background-color: #276140; }
"""
DEFAULT_SIZE = (1180, 800)
ZOOM_STEPS = (0.5, 0.67, 0.8, 0.9, 1.0, 1.1, 1.25, 1.5, 1.75, 2.0)
# Context menu entries the window keeps: moving around and editing text. Saving, opening or downloading images, links
# and media, and the inspector, are left out (TheColor.com's terms forbid saving its pictures).
MENU_ACTIONS = ("GO_BACK", "GO_FORWARD", "STOP", "RELOAD", "COPY", "CUT", "PASTE", "PASTE_AS_PLAIN_TEXT", "DELETE",
                "SELECT_ALL", "INPUT_METHODS", "UNICODE", "INSERT_EMOJI", "SPELLING_GUESS", "NO_GUESSES_FOUND",
                "IGNORE_SPELLING", "LEARN_SPELLING", "IGNORE_GRAMMAR", "COPY_LINK_TO_CLIPBOARD", "MEDIA_PLAY",
                "MEDIA_PAUSE", "MEDIA_MUTE", "TOGGLE_MEDIA_CONTROLS", "TOGGLE_MEDIA_LOOP")


def find_app(app_id: str, catalog_file: Path | None = None) -> App:
    catalog = load_catalog(catalog_file or data_dir() / "catalog.json", recipe_dir=None)
    app = catalog.apps.get(app_id)
    if app is None or app.method != "web":
        raise CatalogError(f"{app_id} is not a website of the App Center")
    return app


def main(argv: list[str] | None = None) -> int:
    argv = sys.argv[1:] if argv is None else argv
    if len(argv) != 1 or not ID_RE.fullmatch(argv[0]):
        print("usage: caramos-web-app <app-id>", file=sys.stderr)
        return 2
    try:
        app = find_app(argv[0])
    except CatalogError as exc:
        print(exc, file=sys.stderr)
        return 1
    if not app.web_window:
        # The site's terms do not allow a window of ours (Polypad's): the launcher opens it in the user's browser.
        return 0 if webapp.open_in_browser(app.web_url) else 1

    # Before WebKit starts its processes, which inherit the environment.
    os.environ.update(webapp.webkit_environment(os.environ, renderer.gpu_drivers(), renderer.memory_kib()))
    import gi

    gi.require_version("Gtk", "3.0")
    gi.require_version("Gdk", "3.0")
    gi.require_version("WebKit2", "4.1")
    from gi.repository import Gdk, GLib, Gtk

    GLib.set_prgname(webapp.window_class(app.id))
    Gdk.set_program_class(webapp.window_class(app.id))
    GLib.set_application_name(app.name)
    application = Gtk.Application(application_id=webapp.application_id(app.id))

    def activate(application_: Gtk.Application) -> None:
        window = application_.get_active_window()
        if window is not None:
            window.present()
            return
        WebAppWindow(application_, app).present()

    application.connect("activate", activate)
    return application.run([sys.argv[0]])


class WebAppWindow:
    """One site in a window: a header bar with Back, Start page and Reload, the page, and a page for errors."""

    def __init__(self, application, app: App) -> None:
        from gi.repository import Gdk, Gio, GLib, Gtk, WebKit2

        self.Gdk, self.Gio, self.GLib, self.Gtk, self.WebKit2 = Gdk, Gio, GLib, Gtk, WebKit2
        self.app = app
        self.failed_uri = ""
        css = Gtk.CssProvider()
        css.load_from_data(CSS)
        Gtk.StyleContext.add_provider_for_screen(Gdk.Screen.get_default(), css, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
        self.window = Gtk.ApplicationWindow(application=application, title=app.name)
        self.window.set_default_size(*DEFAULT_SIZE)
        self.window.maximize()
        icon = webapp.ICON_DIR / app.icon if app.icon else None
        if icon is not None and icon.is_file():
            self.window.set_icon_from_file(str(icon))
        else:
            self.window.set_icon_name(app.icon_name or "applications-internet")

        self.view = self._web_view()
        self.stack = Gtk.Stack(transition_type=Gtk.StackTransitionType.CROSSFADE)
        self.stack.add_named(self.view, "page")
        self.stack.add_named(self._error_page(), "error")
        self.window.add(self.stack)
        self.window.set_titlebar(self._header())
        self._actions(application)
        self.window.show_all()
        self.stack.set_visible_child_name("page")
        self._block_ads_then(self._go_home)

    def present(self) -> None:
        self.window.present()

    # The page --------------------------------------------------------------------------------------------------

    def _web_view(self):
        GLib, WebKit2 = self.GLib, self.WebKit2
        data = Path(GLib.get_user_data_dir()) / "caramos-web-app" / self.app.id
        cache = Path(GLib.get_user_cache_dir()) / "caramos-web-app" / self.app.id
        manager = WebKit2.WebsiteDataManager(base_data_directory=str(data), base_cache_directory=str(cache))
        manager.set_itp_enabled(True)
        manager.get_cookie_manager().set_accept_policy(WebKit2.CookieAcceptPolicy.NO_THIRD_PARTY)
        context = WebKit2.WebContext.new_with_website_data_manager(manager)
        # Little caching: school computers have 2 GB of memory.
        context.set_cache_model(WebKit2.CacheModel.DOCUMENT_BROWSER)
        context.connect("download-started", lambda _context, download: download.cancel())
        self.content = WebKit2.UserContentManager()
        view = WebKit2.WebView(web_context=context, user_content_manager=self.content)
        settings = view.get_settings()
        settings.set_enable_developer_extras(False)
        settings.set_javascript_can_open_windows_automatically(False)
        settings.set_enable_media_stream(False)
        settings.set_enable_webrtc(False)
        settings.set_allow_file_access_from_file_urls(False)
        settings.set_allow_universal_access_from_file_urls(False)
        settings.set_enable_write_console_messages_to_stdout(False)
        settings.set_enable_back_forward_navigation_gestures(True)
        view.connect("decide-policy", self._on_decide_policy)
        view.connect("create", self._on_create)
        view.connect("permission-request", self._on_permission_request)
        view.connect("show-notification", lambda *_args: True)
        view.connect("context-menu", self._on_context_menu)
        view.connect("load-changed", self._on_load_changed)
        view.connect("load-failed", self._on_load_failed)
        view.connect("load-failed-with-tls-errors", self._on_tls_error)
        # A page that crashed (or ran out of memory) waits for Try again, so it cannot crash again and again.
        view.connect("web-process-terminated", lambda *_args: self._show_error(
            _("The page stopped working"), _("Press Try again to open it again.")))
        view.connect("notify::uri", lambda *_args: self._sync_buttons())
        return view

    def _block_ads_then(self, then) -> None:
        """Add the compiled block list (kept in ~/.cache/caramos-web-app/filters, compiled again when the list changes)
        before the first page loads. Without it the site still opens, with its ads."""

        if not self.app.block_ads:
            then()
            return
        GLib, WebKit2 = self.GLib, self.WebKit2
        try:
            rules = json.dumps(webapp.content_rules(webapp.load_blocklist(data_dir() / "web-blocklist.txt")))
        except (OSError, ValueError) as exc:
            print(f"caramos-web-app: no ad blocking: {exc}", file=sys.stderr)
            then()
            return
        identifier = FILTER_PREFIX + hashlib.sha256(rules.encode()).hexdigest()[:16]
        store = WebKit2.UserContentFilterStore.new(str(Path(GLib.get_user_cache_dir()) / "caramos-web-app" / "filters"))

        def use(content_filter) -> None:
            self.content.add_filter(content_filter)
            then()
            store.fetch_identifiers(None, forget_old)

        def saved(store_, result) -> None:
            try:
                content_filter = store_.save_finish(result)
            except GLib.Error as exc:
                print(f"caramos-web-app: no ad blocking: {exc.message}", file=sys.stderr)
                then()
                return
            use(content_filter)

        def loaded(store_, result) -> None:
            try:
                content_filter = store_.load_finish(result)
            except GLib.Error:   # not compiled yet, or an older list
                store_.save(identifier, GLib.Bytes.new(rules.encode()), None, saved)
                return
            use(content_filter)

        def forget_old(store_, result) -> None:
            try:
                names = store_.fetch_identifiers_finish(result)
            except GLib.Error:
                return
            for name in names or []:
                if name.startswith(FILTER_PREFIX) and name != identifier:
                    store_.remove(name, None, None)

        store.load(identifier, None, loaded)

    def _go_home(self, *_args) -> None:
        self.stack.set_visible_child_name("page")
        self.view.load_uri(self.app.web_url)

    def _open_outside(self, url: str) -> None:
        try:
            self.Gtk.show_uri_on_window(self.window, url, self.Gtk.get_current_event_time())
        except self.GLib.Error as exc:
            print(f"caramos-web-app: cannot open {url}: {exc.message}", file=sys.stderr)

    def _on_decide_policy(self, view, decision, decision_type) -> bool:
        WebKit2 = self.WebKit2
        if decision_type == WebKit2.PolicyDecisionType.NAVIGATION_ACTION:
            action = decision.get_navigation_action()
            url = action.get_request().get_uri()
            kind = action.get_navigation_type()
            user = action.is_user_gesture() or kind in (WebKit2.NavigationType.LINK_CLICKED,
                                                        WebKit2.NavigationType.FORM_SUBMITTED)
            choice = webapp.navigation_choice(url, self.app.web_hosts, user_action=user)
            if choice == webapp.STAY:
                return False
            decision.ignore()
            if choice == webapp.BROWSER:
                self._open_outside(url)
            return True
        if decision_type == WebKit2.PolicyDecisionType.NEW_WINDOW_ACTION:
            action = decision.get_navigation_action()
            self._new_window(action.get_request().get_uri(), action.is_user_gesture())
            decision.ignore()
            return True
        if decision_type == WebKit2.PolicyDecisionType.RESPONSE and not decision.is_mime_type_supported():
            decision.ignore()   # a file to download: this window downloads nothing
            return True
        return False

    def _new_window(self, url: str, user_action: bool) -> None:
        choice = webapp.new_window_choice(url, self.app.web_hosts, user_action=user_action)
        if choice == webapp.HERE:
            self.view.load_uri(url)
        elif choice == webapp.BROWSER:
            self._open_outside(url)

    def _on_create(self, _view, action):
        # Reached only when decide-policy did not settle a new window: the same rules, and never a second window.
        self._new_window(action.get_request().get_uri(), action.is_user_gesture())
        return None

    @staticmethod
    def _on_permission_request(_view, request) -> bool:
        # No camera, microphone, location, notifications or anything else a page may ask for.
        request.deny()
        return True

    def _on_context_menu(self, _view, menu, *_args) -> bool:
        keep = {getattr(self.WebKit2.ContextMenuAction, name) for name in MENU_ACTIONS
                if hasattr(self.WebKit2.ContextMenuAction, name)}
        for item in list(menu.get_items()):
            if not item.is_separator() and item.get_stock_action() not in keep:
                menu.remove(item)
        items = menu.get_items()
        # No separator at either end or twice in a row.
        previous_separator = True
        for item in list(items):
            if item.is_separator() and previous_separator:
                menu.remove(item)
            else:
                previous_separator = item.is_separator()
        items = menu.get_items()
        if items and items[-1].is_separator():
            menu.remove(items[-1])
        return not menu.get_items()

    def _on_load_changed(self, _view, event) -> None:
        if event == self.WebKit2.LoadEvent.COMMITTED:
            self.stack.set_visible_child_name("page")
        self._sync_buttons()

    def _on_load_failed(self, _view, _event, uri, error) -> bool:
        WebKit2 = self.WebKit2
        # Navigations this window stopped itself (a link given to the browser, a download) are not errors.
        if error.matches(WebKit2.policy_error_quark(), WebKit2.PolicyError.FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE) or \
                error.matches(WebKit2.network_error_quark(), WebKit2.NetworkError.CANCELLED) or \
                error.matches(WebKit2.download_error_quark(), WebKit2.DownloadError.CANCELLED_BY_USER):
            return False
        self.failed_uri = uri or ""
        monitor = self.Gio.NetworkMonitor.get_default()
        if not monitor.get_network_available() or \
                monitor.get_connectivity() == self.Gio.NetworkConnectivity.LOCAL:
            self._show_error(_("No Internet connection"), _("Connect to Wi-Fi or a network cable, then try again."))
        else:
            self._show_error(_("The website did not open"), _("It did not answer. Try again in a moment."))
        return True

    def _on_tls_error(self, _view, _uri, _certificate, _errors) -> bool:
        # Never past a bad certificate: that is not the real site. Try again starts from the site's first page.
        self.failed_uri = ""
        self._show_error(_("The website did not open"),
                         _("Its security certificate is not valid, so this may not be the real site."))
        return True

    # Window parts ----------------------------------------------------------------------------------------------

    def _header(self):
        Gtk = self.Gtk
        header = Gtk.HeaderBar(title=self.app.name, show_close_button=True)
        self.back = self._button("go-previous-symbolic", _("Back"), lambda *_a: self.view.go_back())
        header.pack_start(self.back)
        header.pack_start(self._button("go-home-symbolic", _("Start page"), self._go_home))
        header.pack_end(self._button("view-refresh-symbolic", _("Reload"), lambda *_a: self._reload()))
        self._sync_buttons()
        return header

    def _button(self, icon: str, tooltip: str, on_clicked):
        button = self.Gtk.Button.new_from_icon_name(icon, self.Gtk.IconSize.BUTTON)
        button.set_tooltip_text(tooltip)
        button.get_accessible().set_name(tooltip)
        button.connect("clicked", on_clicked)
        return button

    def _error_page(self):
        Gtk = self.Gtk
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=12, halign=Gtk.Align.CENTER,
                      valign=Gtk.Align.CENTER, margin=24)
        image = Gtk.Image.new_from_icon_name("network-offline-symbolic", Gtk.IconSize.DIALOG)
        image.set_pixel_size(64)
        image.get_style_context().add_class("dim-label")
        self.error_title = Gtk.Label(wrap=True, justify=Gtk.Justification.CENTER, max_width_chars=50)
        self.error_text = Gtk.Label(wrap=True, justify=Gtk.Justification.CENTER, max_width_chars=50)
        self.error_text.get_style_context().add_class("dim-label")
        retry = Gtk.Button(label=_("Try again"), halign=Gtk.Align.CENTER, margin_top=12)
        retry.get_style_context().add_class("suggested-action")
        retry.connect("clicked", lambda *_a: self._reload())
        for widget in (image, self.error_title, self.error_text, retry):
            box.pack_start(widget, False, False, 0)
        return box

    def _show_error(self, title: str, text: str) -> None:
        self.error_title.set_markup(f"<span size='x-large' weight='600'>{self.GLib.markup_escape_text(title)}</span>")
        self.error_text.set_text(text)
        self.stack.set_visible_child_name("error")

    def _reload(self) -> None:
        # After an error, the page that failed: the window still shows the one before it.
        failed, self.failed_uri = self.failed_uri, ""
        showing_error = self.stack.get_visible_child_name() == "error"
        self.stack.set_visible_child_name("page")
        if showing_error and failed and webapp.is_allowed(failed, self.app.web_hosts):
            self.view.load_uri(failed)
        elif self.view.get_uri():
            self.view.reload()
        else:
            self._go_home()

    def _sync_buttons(self) -> None:
        if getattr(self, "back", None) is not None:
            self.back.set_sensitive(self.view.can_go_back())

    def _actions(self, application) -> None:
        Gio = self.Gio
        actions = {
            "back": (lambda: self.view.go_back(), ["<Alt>Left"]),
            "forward": (lambda: self.view.go_forward(), ["<Alt>Right"]),
            "home": (self._go_home, ["<Alt>Home"]),
            "reload": (self._reload, ["F5", "<Control>r"]),
            "zoom-in": (lambda: self._zoom(1), ["<Control>plus", "<Control>equal", "<Control>KP_Add"]),
            "zoom-out": (lambda: self._zoom(-1), ["<Control>minus", "<Control>KP_Subtract"]),
            "zoom-reset": (lambda: self.view.set_zoom_level(1.0), ["<Control>0", "<Control>KP_0"]),
            "fullscreen": (self._toggle_fullscreen, ["F11"]),
            "close": (self.window.close, ["<Control>w", "<Control>q"]),
        }
        for name, (callback, accels) in actions.items():
            action = Gio.SimpleAction.new(name, None)
            action.connect("activate", lambda _action, _param, run=callback: run())
            self.window.add_action(action)
            application.set_accels_for_action(f"win.{name}", accels)

    def _zoom(self, step: int) -> None:
        level = self.view.get_zoom_level()
        index = min(range(len(ZOOM_STEPS)), key=lambda i: abs(ZOOM_STEPS[i] - level))
        self.view.set_zoom_level(ZOOM_STEPS[max(0, min(len(ZOOM_STEPS) - 1, index + step))])

    def _toggle_fullscreen(self) -> None:
        window = self.window.get_window()
        if window is not None and window.get_state() & self.Gdk.WindowState.FULLSCREEN:
            self.window.unfullscreen()
        else:
            self.window.fullscreen()
