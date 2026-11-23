"""The App Center looks like part of CaramOS (DESIGN.md in the CaramOS repository): libadwaita's own surfaces with
the forest green accent, the system font in two weights, rows in boxed lists instead of cards and chips, and a
first-run picker whose steps are radio buttons or check boxes."""

from __future__ import annotations

import os
import re
import unittest
from pathlib import Path
from unittest import mock

from caramos_app_center.catalog import load_catalog

ROOT = Path(__file__).parents[1]
DATA = ROOT / "usr/share/caramos-app-center"
UI = ROOT / "usr/lib/python3/dist-packages/caramos_app_center/ui"
CATALOG = load_catalog(DATA / "catalog.json", DATA / "recipes")


def _gtk() -> bool:
    try:
        import gi

        gi.require_version("Gtk", "4.0")
        gi.require_version("Adw", "1")
        from gi.repository import Adw, Gdk, Gtk
    except (ImportError, ValueError):
        return False
    if not Gtk.init_check() or Gdk.Display.get_default() is None:
        return False
    Adw.init()
    return True


GTK = _gtk()


class StylesheetTests(unittest.TestCase):
    def colors(self, name: str) -> dict[str, str]:
        return dict(re.findall(r"@define-color\s+(\w+)\s+([^;]+);", (DATA / name).read_text(encoding="utf-8")))

    def test_the_accent_is_the_only_colour_of_caramos(self) -> None:
        for name, accent in (("style.css", ("#2b6a44", "#ffffff", "#276140")),
                             ("style-dark.css", ("#3a855a", "#ffffff", "#8ccfa5"))):
            colors = self.colors(name)
            self.assertEqual(accent, (colors["accent_bg_color"], colors["accent_fg_color"], colors["accent_color"]))
            # No header bar, window or card colours of its own: libadwaita's surfaces stay.
            self.assertEqual({"accent_bg_color", "accent_fg_color", "accent_color", "caramos_line_color"}, set(colors))

    def test_system_font_in_two_weights_without_decoration(self) -> None:
        for name in ("style.css", "style-dark.css"):
            text = (DATA / name).read_text(encoding="utf-8")
            self.assertNotIn("font-family", text)
            self.assertNotIn("gradient", text)
            self.assertLessEqual(set(re.findall(r"font-weight:\s*([^;]+);", text)), {"600", "inherit"})
            self.assertNotRegex(text, r"(?m)^\s*(headerbar|window)\s*[{,]")
            for shadow in re.findall(r"box-shadow:\s*([^;]+);", text):
                self.assertEqual("0 0 0 1px @caramos_line_color", shadow, "lists get a thin line, nothing gets a shadow")


class SourceTests(unittest.TestCase):
    def test_rows_instead_of_cards_chips_and_badges(self) -> None:
        for path in sorted(UI.glob("*.py")):
            text = path.read_text(encoding="utf-8")
            for word in ('"badge"', '"chip"', '"hero', '-card"', '"section-number"'):
                self.assertNotIn(word, text, path.name)

    def test_pill_buttons_only_on_status_pages(self) -> None:
        for path in sorted(UI.glob("*.py")):
            if path.name != "window.py":
                self.assertNotIn('"pill"', path.read_text(encoding="utf-8"), path.name)

    def test_sizes_are_words_not_joined_with_dots(self) -> None:
        po = (ROOT / "po/caramos-app-center/vi.po").read_text(encoding="utf-8")
        self.assertNotIn(" · ", po)
        self.assertIn('msgstr "Từ Flathub, khoảng %s."', po)


@unittest.skipUnless(GTK, "needs GTK 4 and libadwaita with a display")
class WidgetTests(unittest.TestCase):
    def rows(self, widget, kind):
        found = []
        child = widget.get_first_child()
        while child is not None:
            if isinstance(child, kind):
                found.append(child)
            found += self.rows(child, kind)
            child = child.get_next_sibling()
        return found

    def picker(self, installed: set[str]):
        from caramos_app_center.ui.wizard import WizardView

        return WizardView(CATALOG, installed, on_install=lambda ids: None, on_remove=lambda app: None,
                          on_finish=lambda: None, on_later=None)

    def test_the_picker_offers_radio_buttons_check_boxes_and_installed_rows(self) -> None:
        from caramos_app_center.ui.rows import ChoiceRow, InstalledRow, NoneRow

        picker = self.picker({"firefox", "vlc"})
        choices = {row.app.id: row for row in self.rows(picker, ChoiceRow)}
        installed = {row.app.id for row in self.rows(picker, InstalledRow)}
        self.assertEqual({"firefox", "vlc"}, installed)
        self.assertNotIn("vlc", choices)
        self.assertTrue(choices["onlyoffice"].single and not choices["telegram"].single)
        self.assertEqual(1, len(self.rows(picker, NoneRow)))   # the office suite step can be left out
        self.assertEqual(["google-chrome", "ms-fonts", "onlyoffice", "zalo-full"], sorted(picker.selected_ids()))

    def test_activating_a_row_picks_its_app(self) -> None:
        from caramos_app_center.ui.rows import ChoiceRow

        picker = self.picker(set())
        choices = {row.app.id: row for row in self.rows(picker, ChoiceRow)}
        choices["libreoffice"].activate_choice()
        choices["libreoffice"].activate_choice()   # a radio button stays on
        choices["telegram"].activate_choice()
        choices["google-chrome"].activate_choice()  # a check box flips
        self.assertEqual(["libreoffice", "ms-fonts", "telegram", "vlc", "zalo-full"], sorted(picker.selected_ids()))
        self.assertIn("5", picker.install_button.get_label())

    def test_where_an_app_comes_from_reads_as_a_sentence(self) -> None:
        from caramos_app_center.plan import format_size
        from caramos_app_center.ui.common import source_sentence

        for app in CATALOG.apps.values():
            sentence = source_sentence(app)
            self.assertTrue(sentence.endswith("."), sentence)
            if not app.preinstalled:
                self.assertIn(format_size(app.size_mb), sentence)

    def test_search_ignores_vietnamese_accents(self) -> None:
        from caramos_app_center.ui.store import StoreView

        store = StoreView(CATALOG, set(), on_action=lambda app, action: None)
        with mock.patch.dict(os.environ, {"LANGUAGE": "vi_VN:vi"}):
            store.query = "trinh duyet"
            found = {app.id for app in store.visible_apps()}
        self.assertLessEqual({"google-chrome", "coccoc", "microsoft-edge", "brave", "firefox"}, found)


if __name__ == "__main__":
    unittest.main()
