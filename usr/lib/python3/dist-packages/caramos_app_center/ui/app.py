"""Application entry point.

    caramos-app-center              open the store
    caramos-app-center --wizard     open the app picker
    caramos-app-center --first-run  autostart: open the app picker once, only where first run applies
"""

from __future__ import annotations

import argparse
import sys

from ..i18n import _

from .. import firstrun
from ..catalog import CatalogError, data_dir, load_catalog

APP_ID = "net.vietnamlinuxfamily.CaramOSAppCenter"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="caramos-app-center")
    parser.add_argument("--wizard", action="store_true", help="open the first-run app picker")
    parser.add_argument("--first-run", action="store_true", help="autostart mode: picker once per user")
    args = parser.parse_args(sys.argv[1:] if argv is None else argv)

    if args.first_run and not firstrun.should_run():
        return 0
    try:
        # The UI does not check that recipes exist; the helper does before running anything.
        catalog = load_catalog(data_dir() / "catalog.json", recipe_dir=None)
    except CatalogError as exc:
        print(exc, file=sys.stderr)
        return 1

    import gi

    gi.require_version("Gtk", "4.0")
    gi.require_version("Adw", "1")
    from gi.repository import Adw, GLib

    from .common import load_css
    from .window import MainWindow

    GLib.set_prgname("caramos-app-center")
    GLib.set_application_name(_("CaramOS App Center"))
    application = Adw.Application(application_id=APP_ID)

    def activate(app) -> None:
        if app.get_active_window() is not None:
            app.get_active_window().present()
            return
        load_css()
        window = MainWindow(app, catalog, wizard=args.wizard or args.first_run, first_run=args.first_run)
        window.present()

    application.connect("activate", activate)
    return application.run([sys.argv[0]])
