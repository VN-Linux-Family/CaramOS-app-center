"""Application entry point.

    caramos-app-center              open the app picker (the store list is hidden for now)
    caramos-app-center --first-run  autostart: open the app picker once, only where first run applies
    caramos-app-center --store      developers: open the store list, which needs the VNLF app site API
                                    before it ships again
    caramos-app-center caramos-app://install/<Flathub app id>
                                    an install link of the VNLF app site (caramos-app-center-link.desktop hands it
                                    over): the app's page. A running App Center takes it, in its own window.
"""

from __future__ import annotations

import argparse
import sys

from ..i18n import _

from .. import firstrun, renderer
from ..catalog import CatalogError, data_dir, load_catalog

APP_ID = "net.vietnamlinuxfamily.CaramOSAppCenter"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="caramos-app-center")
    # --wizard was the picker's flag while the store was the default; still accepted.
    parser.add_argument("--wizard", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--first-run", action="store_true", help="autostart mode: picker once per user")
    parser.add_argument("--store", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("link", nargs="?", help="an install link, caramos-app://install/<Flathub app id>")
    args = parser.parse_args(sys.argv[1:] if argv is None else argv)

    if args.first_run and not firstrun.should_run():
        return 0
    try:
        # The UI does not check that recipes exist; the helper does before running anything.
        catalog = load_catalog(data_dir() / "catalog.json", recipe_dir=None)
    except CatalogError as exc:
        print(exc, file=sys.stderr)
        return 1

    renderer.apply()  # cairo where OpenGL would run on the CPU
    import gi

    gi.require_version("Gtk", "4.0")
    gi.require_version("Adw", "1")
    from gi.repository import Adw, Gio, GLib

    from .common import load_css
    from .window import MainWindow

    GLib.set_prgname("caramos-app-center")
    GLib.set_application_name(_("CaramOS App Center"))
    application = Adw.Application(application_id=APP_ID)
    # The link to open when this process makes the window (the window checks it; an invalid one gets an error page).
    pending = {"link": args.link}

    def activate(app) -> None:
        if app.get_active_window() is not None:
            app.get_active_window().present()
            return
        load_css()
        link, pending["link"] = pending["link"], None
        window = MainWindow(app, catalog, store=args.store and not args.first_run and link is None,
                            first_run=args.first_run and link is None, link=link)
        window.present()

    def open_link(_action, parameter) -> None:
        window = application.get_active_window()
        if window is None:
            pending["link"] = parameter.get_string()
            application.activate()
        else:
            window.open_link(parameter.get_string())
            window.present()

    # Install links reach the App Center that is already running through this action (GApplication forwards it over
    # D-Bus), so a link never opens a second window.
    action = Gio.SimpleAction.new("open-link", GLib.VariantType.new("s"))
    action.connect("activate", open_link)
    application.add_action(action)
    application.connect("activate", activate)
    if args.link is not None:
        try:
            application.register(None)
        except GLib.Error as exc:
            print(exc.message, file=sys.stderr)
            return 1
        if application.get_is_remote():
            application.activate_action("open-link", GLib.Variant("s", args.link))
            connection = application.get_dbus_connection()
            if connection is not None:
                connection.flush_sync(None)
            return 0
    return application.run([sys.argv[0]])
