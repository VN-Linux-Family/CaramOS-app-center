"""Tests for the App Center catalog: the shipped file and the validation rules."""

from __future__ import annotations

import copy
import json
import unittest
from pathlib import Path

from caramos_app_center.catalog import CatalogError, load_catalog, localized, more_apps_url, parse_catalog

ROOT = Path(__file__).parents[1]
DATA = ROOT / "usr/share/caramos-app-center"
CATALOG = DATA / "catalog.json"
RECIPES = DATA / "recipes"


class ShippedCatalogTests(unittest.TestCase):
    def setUp(self) -> None:
        self.catalog = load_catalog(CATALOG, RECIPES)

    def test_loads_with_every_recipe_present_and_executable(self) -> None:
        recipes = {app.recipe for app in self.catalog.apps.values() if app.method == "recipe"}
        self.assertEqual({"wps-office", "google-chrome", "coccoc", "microsoft-edge", "zalo-full", "zoom", "ms-fonts"},
                         recipes)

    def test_first_run_steps_offer_the_choices_from_issue_46(self) -> None:
        steps = {step.id: step for step in self.catalog.steps}
        self.assertEqual(["office", "browser", "chat", "extras"], [step.id for step in self.catalog.steps])
        self.assertEqual({"wps-office", "onlyoffice", "libreoffice"}, set(steps["office"].apps))
        self.assertEqual("single", steps["office"].choice)
        self.assertTrue(steps["office"].allow_none)
        self.assertIn("zalo-full", steps["chat"].apps)
        self.assertEqual(("google-chrome",), steps["browser"].default)

    def test_every_app_has_an_icon_and_both_languages(self) -> None:
        for app in self.catalog.apps.values():
            if app.icon:
                self.assertTrue((DATA / "icons" / app.icon).is_file(), app.id)
            else:
                self.assertTrue(app.icon_name, app.id)
            for language in ("vi", "en"):
                self.assertTrue(app.summary[language], app.id)
                self.assertTrue(app.pros[language] and app.cons[language], app.id)

    def test_install_sources_follow_the_flathub_first_rule(self) -> None:
        # Flathub where the publisher-verified app exists, the publisher's package otherwise.
        methods = {app.id: app.method for app in self.catalog.apps.values()}
        for app_id in ("onlyoffice", "libreoffice", "brave", "telegram", "discord", "obs-studio"):
            self.assertEqual("flatpak", methods[app_id], app_id)
        for app_id in ("wps-office", "google-chrome", "microsoft-edge", "coccoc", "zoom"):
            self.assertEqual("recipe", methods[app_id], app_id)
        self.assertEqual("preinstalled", methods["firefox"])

    def test_more_apps_link_goes_to_the_vnlf_app_site(self) -> None:
        self.assertEqual("https://apps.vietnamlinuxfamily.net/browse", more_apps_url())
        self.assertEqual("https://apps.vietnamlinuxfamily.net/browse?q=k%E1%BA%BF+to%C3%A1n", more_apps_url(" kế toán "))

    def test_localized_follows_the_session_language(self) -> None:
        text = {"vi": "Văn phòng", "en": "Office"}
        self.assertEqual("Văn phòng", localized(text, {"LANG": "vi_VN.UTF-8"}))
        self.assertEqual("Office", localized(text, {"LANG": "en_US.UTF-8"}))


class ValidationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.data = json.loads(CATALOG.read_text(encoding="utf-8"))

    def broken(self, mutate) -> None:
        data = copy.deepcopy(self.data)
        mutate(data)
        with self.assertRaises(CatalogError):
            parse_catalog(data, RECIPES)

    def test_rejects_bad_ids_and_methods(self) -> None:
        self.broken(lambda d: d["apps"][0].update(id="../etc"))
        self.broken(lambda d: d["apps"][0].update(install={"flatpak": "org.x.Y", "apt": ["x"]}))
        self.broken(lambda d: d["apps"][0].update(install={"flatpak": "not a ref"}))
        self.broken(lambda d: d["apps"][0].update(install={"apt": ["bad name!"]}))
        self.broken(lambda d: d["apps"][1].update(install={"recipe": "does-not-exist"}))

    def test_rejects_inconsistent_steps(self) -> None:
        self.broken(lambda d: d["steps"][0].update(apps=["unknown-app"]))
        self.broken(lambda d: d["steps"][0].update(default=["telegram"]))
        self.broken(lambda d: d["steps"][0].update(default=["onlyoffice", "libreoffice"]))

    def test_rejects_missing_translation(self) -> None:
        self.broken(lambda d: d["apps"][0]["summary"].pop("en"))
        self.broken(lambda d: d["apps"][0]["pros"].update(vi=[]))

    def test_rejects_duplicate_ids(self) -> None:
        self.broken(lambda d: d["apps"].append(copy.deepcopy(d["apps"][0])))


if __name__ == "__main__":
    unittest.main()
