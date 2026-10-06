"""Packaging checks for the App Center: installed paths, polkit, launchers, recipes and the ISO flag."""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import unittest
from pathlib import Path

from caramos_app_center import runner

ROOT = Path(__file__).parents[1]
RECIPES = ROOT / "usr/share/caramos-app-center/recipes"


class PackagingTests(unittest.TestCase):
    def test_debian_install_ships_every_app_center_path(self) -> None:
        install = (ROOT / "debian/install").read_text(encoding="utf-8")
        for line in (
            "usr/bin/caramos-app-center usr/bin/",
            "usr/lib/caramos-app-center usr/lib/",
            "usr/lib/python3/dist-packages/caramos_app_center usr/lib/python3/dist-packages/",
            "usr/share/caramos-app-center usr/share/",
            "usr/share/applications/caramos-app-center.desktop usr/share/applications/",
            "usr/share/polkit-1/actions/net.vietnamlinuxfamily.caramos-app-center.policy usr/share/polkit-1/actions/",
            "usr/share/locale usr/share/",
            "etc/xdg/autostart/caramos-app-center-first-run.desktop etc/xdg/autostart/",
        ):
            self.assertIn(line, install)

    def test_runtime_dependencies(self) -> None:
        control = (ROOT / "debian/control").read_text(encoding="utf-8")
        for package in ("gir1.2-gtk-4.0", "gir1.2-adw-1", "gir1.2-flatpak-1.0", "flatpak", "curl", "gnupg", "pkexec"):
            self.assertRegex(control, rf"(?m)^ {re.escape(package)},?$")

    def test_polkit_action_points_at_the_helper(self) -> None:
        policy = (ROOT / "usr/share/polkit-1/actions/net.vietnamlinuxfamily.caramos-app-center.policy").read_text(encoding="utf-8")
        self.assertIn(f'<annotate key="org.freedesktop.policykit.exec.path">{runner.HELPER_PATH}</annotate>', policy)
        self.assertIn("<allow_active>auth_admin_keep</allow_active>", policy)
        helper = ROOT / runner.HELPER_PATH.lstrip("/")
        self.assertTrue(helper.is_file() and os.access(helper, os.X_OK))
        self.assertTrue(helper.read_text(encoding="utf-8").startswith("#!/usr/bin/python3 -I\n"))

    def test_launchers(self) -> None:
        menu = (ROOT / "usr/share/applications/caramos-app-center.desktop").read_text(encoding="utf-8")
        autostart = (ROOT / "etc/xdg/autostart/caramos-app-center-first-run.desktop").read_text(encoding="utf-8")
        self.assertIn("Exec=/usr/bin/caramos-app-center\n", menu)
        self.assertIn("Exec=/usr/bin/caramos-app-center --first-run\n", autostart)
        self.assertIn("NoDisplay=true", autostart)

    def test_translations_are_compiled_at_build_time(self) -> None:
        rules = (ROOT / "debian/rules").read_text(encoding="utf-8")
        makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
        self.assertIn("$(MAKE) compile", rules)
        self.assertIn('msgfmt --check --output-file="usr/share/locale/$$lang/LC_MESSAGES/$(DOMAIN).mo"', makefile)
        self.assertIn("gettext", (ROOT / "debian/control").read_text(encoding="utf-8"))


class RecipeTests(unittest.TestCase):
    def recipes(self) -> list[Path]:
        return sorted(p for p in RECIPES.iterdir() if p.is_file())

    def test_recipes_are_executable_and_take_install_or_remove(self) -> None:
        for recipe in self.recipes():
            self.assertTrue(os.access(recipe, os.X_OK), recipe.name)
            source = recipe.read_text(encoding="utf-8")
            if source.startswith("#!/bin/bash"):
                self.assertIn(". /usr/share/caramos-app-center/recipes/lib/common.sh", source)
                self.assertIn('require_action "${1:-}"', source)
            else:
                self.assertIn('("install", "remove")', source)

    def test_bash_recipes_parse(self) -> None:
        bash = shutil.which("bash")
        if bash is None:
            self.skipTest("bash not installed")
        for recipe in [*self.recipes(), RECIPES / "lib/common.sh"]:
            if recipe.read_text(encoding="utf-8").startswith(("#!/bin/bash", "# Shared")):
                subprocess.run([bash, "-n", str(recipe)], check=True)

    def test_downloads_use_https_only(self) -> None:
        for recipe in self.recipes():
            for url in re.findall(r"""https?://[^\s"')]+""", recipe.read_text(encoding="utf-8")):
                self.assertTrue(url.startswith("https://"), f"{recipe.name}: {url}")


if __name__ == "__main__":
    unittest.main()
