"""Every app in the catalog says what kind of licence it has (CaramOS Doanh nghiệp's licence report reads the
installed catalog: PROTOCOL.md part 3 of caramos-business, "App Center catalog field")."""

from __future__ import annotations

import json
import unittest
from pathlib import Path

CATALOG = Path(__file__).parents[1] / "usr/share/caramos-app-center/catalog.json"
CLASSES = ("free", "freeware", "commercial", "unknown")


class LicenceFieldTests(unittest.TestCase):
    def setUp(self) -> None:
        self.apps = json.loads(CATALOG.read_text(encoding="utf-8"))["apps"]

    def test_every_app_has_a_licence(self) -> None:
        for app in self.apps:
            with self.subTest(app=app["id"]):
                licence = app.get("licence")
                self.assertIsInstance(licence, dict)
                self.assertEqual({"class", "note"}, set(licence))
                self.assertIn(licence["class"], CLASSES)
                self.assertEqual({"vi", "en"}, set(licence["note"]))
                for language in ("vi", "en"):
                    note = licence["note"][language]
                    self.assertTrue(isinstance(note, str) and 0 < len(note) <= 300, language)

    def test_the_classes_of_well_known_apps(self) -> None:
        classes = {app["id"]: app["licence"]["class"] for app in self.apps}
        for app_id in ("libreoffice", "onlyoffice", "firefox", "vlc", "gimp", "thunderbird", "remmina",
                       "caramos-business", "klickety", "ktouch", "kolourpaint", "leocad", "tuxtype", "exelearning",
                       "openconnect"):
            self.assertEqual("free", classes[app_id], app_id)
        for app_id in ("google-chrome", "coccoc", "microsoft-edge", "zalo-full", "zoom", "discord", "ms-fonts",
                       "thecolor", "minimouse"):
            self.assertEqual("freeware", classes[app_id], app_id)
        self.assertEqual("commercial", classes["geogebra"], "free only for non-commercial use")
        self.assertEqual("commercial", classes["polypad-tangram"], "free for personal, educational, non-commercial use")
        self.assertEqual("commercial", classes["anydesk"], "free for personal use only")
        self.assertEqual("commercial", classes["spotify"], "personal, non-commercial use")
        self.assertEqual("unknown", classes["wps-office"])


if __name__ == "__main__":
    unittest.main()
