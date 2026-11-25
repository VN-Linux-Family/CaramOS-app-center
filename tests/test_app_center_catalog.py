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
        self.assertEqual({"wps-office", "google-chrome", "coccoc", "microsoft-edge", "zalo-full", "zoom", "ms-fonts",
                          "exelearning", "anydesk"}, recipes)

    def test_first_run_steps_offer_the_choices_from_issue_46(self) -> None:
        steps = {step.id: step for step in self.catalog.steps}
        self.assertEqual(["office", "browser", "chat", "extras", "media", "education", "teaching", "business"],
                         [step.id for step in self.catalog.steps])
        self.assertEqual({"wps-office", "onlyoffice", "libreoffice"}, set(steps["office"].apps))
        self.assertEqual("single", steps["office"].choice)
        self.assertTrue(steps["office"].allow_none)
        self.assertIn("zalo-full", steps["chat"].apps)
        self.assertEqual(("google-chrome",), steps["browser"].default)
        # CaramOS 1.0.21 ships no Zalo, so the recommended set installs it.
        self.assertEqual(("zalo-full",), steps["chat"].default)
        # The recommended set stays: the Microsoft fonts and VLC, now in two steps.
        self.assertEqual(("ms-fonts",), steps["extras"].default)
        self.assertEqual(("vlc",), steps["media"].default)

    def test_learning_apps_for_school_computers(self) -> None:
        # A teacher's request (2026-11-17): CaramOS for school computer labs. Nothing is picked by default, so a
        # home install does not download them; apt where Ubuntu has a current version (lighter than Flatpak
        # runtimes on 2 GB lab computers), Flathub for Scratch 3 and GeoGebra 6. Two steps since the second request
        # (2026-11-25, the Windows programs Vietnamese schools use): the children's computer skills, then programming,
        # science and the teachers' own tools.
        steps = {step.id: step for step in self.catalog.steps}
        education, teaching = steps["education"], steps["teaching"]
        for step in (education, teaching):
            self.assertEqual((), step.default)
            self.assertTrue(step.allow_none)
            self.assertEqual("multi", step.choice)
            self.assertEqual("education", step.web_category)
        self.assertEqual({"gcompris", "minimouse", "klickety", "tuxpaint", "kolourpaint", "thecolor", "tuxtype", "ktouch",
                          "klavaro", "polypad-tangram"}, set(education.apps))
        self.assertEqual({"turbowarp", "kturtle", "thonny", "geogebra", "stellarium", "leocad", "exelearning"},
                         set(teaching.apps))
        self.assertEqual(set(education.apps) | set(teaching.apps), {app.id for app in self.catalog.apps_in("education")})
        methods = {app_id: self.catalog.app(app_id).method for app_id in (*education.apps, *teaching.apps)}
        self.assertEqual({"turbowarp": "flatpak", "geogebra": "flatpak", "leocad": "flatpak", "exelearning": "recipe",
                          "minimouse": "web", "thecolor": "web", "polypad-tangram": "web"},
                         {app_id: method for app_id, method in methods.items() if method != "apt"})
        self.assertEqual(("org.turbowarp.TurboWarp",), self.catalog.app("turbowarp").targets)
        self.assertEqual(("gcompris-qt",), self.catalog.app("gcompris").targets)

    def test_windows_school_programs_have_a_replacement(self) -> None:
        # What each new app stands in for, in the words its card uses.
        replaces = {"klickety": "Clickomania", "ktouch": "Rapid Typing", "kolourpaint": "Paint", "exelearning": "iSpring"}
        for app_id, program in replaces.items():
            app = self.catalog.app(app_id)
            for language in ("vi", "en"):
                self.assertIn(program, " ".join([app.summary[language], *app.pros[language]]), (app_id, language))
        # Ubuntu's packages: KDE Frameworks 5 like KTurtle and GCompris already use, lighter than the KDE Flatpak runtime.
        self.assertEqual(("klickety",), self.catalog.app("klickety").apt_packages)
        self.assertEqual(("ktouch",), self.catalog.app("ktouch").apt_packages)
        self.assertEqual(("kolourpaint",), self.catalog.app("kolourpaint").apt_packages)
        self.assertEqual(("tuxtype",), self.catalog.app("tuxtype").apt_packages)   # not on Flathub
        # LeoCAD from Flathub (the owner's decision, 2026-11-25): LeoCAD's own 26.09 with its parts library, instead
        # of Ubuntu's 21.06 with a 2023 library.
        self.assertEqual("org.leocad.LeoCAD", self.catalog.app("leocad").flatpak_id)
        self.assertIsNone(self.catalog.app("leocad").detect)
        self.assertEqual({"deb": "exelearning"}, dict(self.catalog.app("exelearning").detect))
        self.assertEqual("developer", self.catalog.app("exelearning").source)

    def test_websites_as_apps(self) -> None:
        sites = {app.id: app for app in self.catalog.apps.values() if app.method == "web"}
        self.assertEqual({"thecolor", "minimouse", "polypad-tangram"}, set(sites))
        # Amplify's terms (2026-11-25, the owner's decision): Polypad opens in the user's browser, not in a window of
        # ours; the two ad-funded sites open in a window of their own with their ads blocked.
        self.assertFalse(sites["polypad-tangram"].web_window)
        self.assertTrue(sites["thecolor"].web_window and sites["minimouse"].web_window)
        for language in ("vi", "en"):
            self.assertIn("browser" if language == "en" else "trình duyệt", sites["polypad-tangram"].summary[language])
        self.assertEqual("https://www.thecolor.com/", sites["thecolor"].web_url)
        self.assertEqual(("thecolor.com",), sites["thecolor"].web_hosts)
        self.assertEqual("https://www.minimouse.us/", sites["minimouse"].web_url)
        self.assertEqual("https://polypad.amplify.com/tangram", sites["polypad-tangram"].web_url)
        self.assertEqual(("polypad.amplify.com",), sites["polypad-tangram"].web_hosts)
        # Amplify's terms: no portion of Polypad may be blocked or obscured. The ad-funded sites' terms do not
        # forbid it.
        self.assertFalse(sites["polypad-tangram"].block_ads)
        self.assertTrue(sites["thecolor"].block_ads and sites["minimouse"].block_ads)
        for app in sites.values():
            self.assertEqual("website", app.source)
            self.assertIsNone(app.detect)
            # Not the sites' logos: TheColor.com's terms forbid copying any part of the site, and Amplify allows its
            # logos only to partners. Icons of the icon theme instead.
            self.assertEqual("", app.icon, app.id)
            self.assertTrue(app.icon_name, app.id)
            for language in ("vi", "en"):
                self.assertIn("Internet", " ".join(app.cons[language]), app.id)

    def test_remote_help_and_the_workplace_vpn(self) -> None:
        steps = {step.id: step for step in self.catalog.steps}
        self.assertLess(steps["extras"].apps.index("rustdesk"), steps["extras"].apps.index("anydesk"))
        self.assertIn("openconnect", steps["extras"].apps)
        anydesk = self.catalog.app("anydesk")
        self.assertEqual(("recipe", "anydesk", {"deb": "anydesk"}), (anydesk.method, anydesk.recipe, dict(anydesk.detect)))
        openconnect = self.catalog.app("openconnect")
        self.assertEqual(("openconnect", "network-manager-openconnect-gnome"), openconnect.apt_packages)
        self.assertEqual({"deb": "network-manager-openconnect-gnome"}, dict(openconnect.detect))
        self.assertIn("Cisco AnyConnect", openconnect.summary["vi"])
        self.assertIn("Cisco AnyConnect", openconnect.summary["en"])

    def test_every_app_can_be_reached_in_the_picker(self) -> None:
        # The store list is hidden (window.py), so an app in no step of the picker cannot be installed.
        in_steps = [app_id for step in self.catalog.steps for app_id in step.apps]
        self.assertEqual(set(self.catalog.apps), set(in_steps))
        self.assertEqual(len(in_steps), len(set(in_steps)), "an app is in one step only")

    def test_spotify_from_flathub_and_no_steam(self) -> None:
        # A user's report (2026-11-25): Spotify and Steam installed straight from their makers did not run. Spotify from
        # Flathub, with the music and video apps, nothing ticked. No Steam: Vietnam blocks it (since May 2024), so it
        # cannot update or sign in however it is installed (the owner's decision, 2026-11-25).
        steps = {step.id: step for step in self.catalog.steps}
        self.assertIn("spotify", steps["media"].apps)
        self.assertNotIn("spotify", steps["media"].default)
        app = self.catalog.app("spotify")
        self.assertEqual(("com.spotify.Client", "media"), (app.flatpak_id, app.category))
        for language in ("vi", "en"):
            self.assertIn("Spotify", " ".join(app.cons[language]))
        icon = DATA / "icons" / app.icon
        self.assertEqual(b"\x89PNG", icon.read_bytes()[:4])
        self.assertEqual((128, 128), (int.from_bytes(icon.read_bytes()[16:20], "big"),
                                      int.from_bytes(icon.read_bytes()[20:24], "big")))
        self.assertNotIn("steam", self.catalog.apps)
        self.assertNotIn("com.valvesoftware.Steam", {app.flatpak_id for app in self.catalog.apps.values()})
        self.assertFalse((DATA / "icons" / "steam.png").exists())

    def test_photos_video_and_design(self) -> None:
        steps = {step.id: step for step in self.catalog.steps}
        self.assertEqual(("vlc", "spotify", "obs-studio", "kdenlive", "gimp", "inkscape"), steps["media"].apps)
        self.assertEqual(("ms-fonts", "thunderbird", "obsidian", "rustdesk", "anydesk", "openconnect"),
                         steps["extras"].apps)
        self.assertEqual("audiovideo", steps["media"].web_category)

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

    def test_each_card_says_where_the_app_comes_from(self) -> None:
        # Zalo for Linux is built by VN-Linux-Family/zalo-for-linux, not by Zalo: its card must not say
        # "From the developer".
        sources = {app.id: app.source for app in self.catalog.apps.values()}
        self.assertEqual("community", sources["zalo-full"])
        for app_id in ("wps-office", "google-chrome", "microsoft-edge", "coccoc", "zoom", "exelearning", "anydesk"):
            self.assertEqual("developer", sources[app_id], app_id)
        self.assertEqual("website", sources["thecolor"])
        self.assertEqual("flathub", sources["telegram"])
        self.assertEqual("ubuntu", sources["vlc"])
        self.assertEqual("caramos", sources["firefox"])
        self.assertEqual("caramos-package", sources["caramos-business"])  # from the CaramOS PPA, not Ubuntu's

    def test_business_apps(self) -> None:
        # CaramOS Doanh nghiệp: the company's wallpaper and shared folders, a package of the CaramOS PPA.
        app = self.catalog.apps["caramos-business"]
        self.assertEqual("business", app.category)
        self.assertEqual(("caramos-business",), app.apt_packages)
        self.assertEqual({"deb": "caramos-business"}, dict(app.detect))

    def test_remmina_reaches_windows_only_software(self) -> None:
        # The Windows migration kit: MISA SME and HTKK run only on Windows, so an office keeps one Windows computer
        # or server and its staff open it from CaramOS over RDP. Remmina from Ubuntu, with the RDP and VNC plugins
        # and the keyring plugin (a Recommends, which Mint does not install) that saves the password.
        app = self.catalog.apps["remmina"]
        self.assertEqual("business", app.category)
        self.assertEqual(("remmina", "remmina-plugin-rdp", "remmina-plugin-vnc", "remmina-plugin-secret"),
                         app.apt_packages)
        self.assertEqual({"deb": "remmina"}, dict(app.detect))
        self.assertEqual("ubuntu", app.source)
        for language in ("vi", "en"):
            text = " ".join([app.summary[language], *app.pros[language], *app.cons[language]])
            self.assertIn("Windows", text)
            self.assertIn("MISA SME", text)
            self.assertIn("HTKK", text)
        icon = DATA / "icons" / app.icon
        self.assertEqual(b"\x89PNG", icon.read_bytes()[:4])
        self.assertEqual((128, 128), (int.from_bytes(icon.read_bytes()[16:20], "big"),
                                      int.from_bytes(icon.read_bytes()[20:24], "big")))

    def test_more_apps_link_goes_to_the_vnlf_app_site(self) -> None:
        # from=caramos: the site knows the visitor runs CaramOS and shows its "Cài trên CaramOS" buttons.
        self.assertEqual("https://apps.vietnamlinuxfamily.net/browse?from=caramos", more_apps_url())
        self.assertEqual("https://apps.vietnamlinuxfamily.net/browse?q=k%E1%BA%BF+to%C3%A1n&from=caramos",
                         more_apps_url(" kế toán "))
        self.assertEqual("https://apps.vietnamlinuxfamily.net/browse?category=network&from=caramos",
                         more_apps_url(category="network"))

    def test_every_step_links_to_a_category_of_the_vnlf_app_site(self) -> None:
        # Slugs of https://apps-api.vietnamlinuxfamily.net/api/categories.
        site = {"network", "development", "graphics", "audiovideo", "office", "game", "system", "education",
                "security", "communication", "science", "utility"}
        for step in self.catalog.steps:
            self.assertIn(step.web_category, site, step.id)

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
        self.broken(lambda d: d["apps"][0].update(caramos="yes"))
        self.broken(lambda d: d["apps"][0].update(install={"flatpak": "org.x.Y"}, caramos=True))  # only apt

    def test_rejects_inconsistent_steps(self) -> None:
        self.broken(lambda d: d["steps"][0].update(apps=["unknown-app"]))
        self.broken(lambda d: d["steps"][0].update(default=["telegram"]))
        self.broken(lambda d: d["steps"][0].update(default=["onlyoffice", "libreoffice"]))
        self.broken(lambda d: d["steps"][0].update(web_category="office&x=1"))

    def test_rejects_missing_translation(self) -> None:
        self.broken(lambda d: d["apps"][0]["summary"].pop("en"))
        self.broken(lambda d: d["apps"][0]["pros"].update(vi=[]))

    def test_rejects_websites_outside_their_hosts(self) -> None:
        def site(**web):
            def mutate(d):
                app = next(a for a in d["apps"] if a["id"] == "thecolor")
                app["install"] = {"web": {"url": "https://www.thecolor.com/", "hosts": ["thecolor.com"], **web}}
            return mutate

        self.broken(site(url="http://www.thecolor.com/"))            # https only
        self.broken(site(url="https://thecolor.com.example.net/"))   # not one of its hosts
        self.broken(site(url="https://user@www.thecolor.com/"))
        self.broken(site(hosts=[]))
        self.broken(site(hosts=["thecolor.com/"]))
        self.broken(site(block_ads="yes"))
        self.broken(site(open="tab"))
        self.broken(site(open="browser", block_ads=True))          # only a window of ours can block anything
        self.broken(site(script="https://example.net/x.js"))          # nothing but url, hosts, block_ads
        self.broken(lambda d: next(a for a in d["apps"] if a["id"] == "thecolor").update(detect={"deb": "x"}))

    def test_community_is_a_flag_of_recipes(self) -> None:
        self.broken(lambda d: d["apps"][0].update(community=True))   # ONLYOFFICE, from Flathub
        self.broken(lambda d: d["apps"][1].update(community="yes"))

    def test_rejects_duplicate_ids(self) -> None:
        self.broken(lambda d: d["apps"].append(copy.deepcopy(d["apps"][0])))


if __name__ == "__main__":
    unittest.main()
