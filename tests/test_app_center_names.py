"""App names in each session language, and the picker reopened after an install with nothing ticked."""

from __future__ import annotations

import os
import unittest
from pathlib import Path
from unittest import mock

from caramos_app_center.catalog import load_catalog

ROOT = Path(__file__).parents[1]
DATA = ROOT / "usr/share/caramos-app-center"
WINDOW = ROOT / "usr/lib/python3/dist-packages/caramos_app_center/ui/window.py"
SESSION_KEYS = ("LANGUAGE", "LC_ALL", "LC_MESSAGES", "LANG")


def catalog_in(language: str):
    environ = {key: value for key, value in os.environ.items() if key not in SESSION_KEYS}
    environ["LANG"] = "vi_VN.UTF-8" if language == "vi" else "en_US.UTF-8"
    with mock.patch.dict(os.environ, environ, clear=True):
        return load_catalog(DATA / "catalog.json", DATA / "recipes")


class NameTests(unittest.TestCase):
    def test_caramos_business_has_an_english_name(self) -> None:
        self.assertEqual("CaramOS Doanh nghiệp", catalog_in("vi").apps["caramos-business"].name)
        self.assertEqual("CaramOS Business", catalog_in("en").apps["caramos-business"].name)

    def test_other_names_are_the_same_in_both_languages(self) -> None:
        vi, en = catalog_in("vi"), catalog_in("en")
        self.assertEqual("Remmina", en.apps["remmina"].name)
        for app_id, app in vi.apps.items():
            if app_id != "caramos-business":
                self.assertEqual(app.name, en.apps[app_id].name, app_id)

    def test_the_business_step_is_reachable_in_the_picker(self) -> None:
        steps = {step.id: step for step in catalog_in("vi").steps}
        self.assertEqual(("caramos-business", "remmina"), steps["business"].apps)
        self.assertEqual((), steps["business"].default)


class ChooseMoreTests(unittest.TestCase):
    def test_choose_more_apps_opens_the_picker_with_nothing_ticked(self) -> None:
        source = WINDOW.read_text(encoding="utf-8")
        self.assertIn('_pill(_("Choose more apps"), lambda: self.show_wizard(selection={}))', source)


if __name__ == "__main__":
    unittest.main()
