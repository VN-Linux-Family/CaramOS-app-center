"""Websites as apps (webapp.py): the launcher the helper writes, where the window lets a navigation go, the block list,
and the helper's install and removal."""

from __future__ import annotations

import io
import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from caramos_app_center import helper, webapp
from caramos_app_center.catalog import load_catalog
from caramos_app_center.detect import installed_apps

ROOT = Path(__file__).parents[1]
DATA = ROOT / "usr/share/caramos-app-center"
CATALOG = load_catalog(DATA / "catalog.json", DATA / "recipes")
RUNNER = ROOT / "usr/lib/caramos-app-center/caramos-web-app"


def no_programs(_args: list[str]) -> str:
    return ""


class LauncherTests(unittest.TestCase):
    def test_launcher_runs_the_window_with_the_app_id_only(self) -> None:
        text = webapp.launcher_text(CATALOG.app("thecolor"))
        lines = text.splitlines()
        self.assertEqual("[Desktop Entry]", lines[0])
        self.assertIn("Exec=/usr/lib/caramos-app-center/caramos-web-app thecolor", lines)
        self.assertIn("Name=TheColor.com", lines)
        self.assertIn("Categories=Education;", lines)
        self.assertIn("StartupWMClass=caramos-web-thecolor", lines)
        self.assertIn("Icon=applications-graphics", lines)
        self.assertTrue(any(line.startswith("Comment[vi]=Tô màu") for line in lines))
        # The address stays in the catalog: the launcher carries nothing the window would trust.
        self.assertNotIn("http", text)

    def test_catalog_text_cannot_add_keys_to_the_launcher(self) -> None:
        app = CATALOG.app("minimouse")
        sneaky = type(app)(**{**app.__dict__, "name": "Mouse\nExec=/bin/sh", "summary": {"vi": "a\nb", "en": "c"}})
        lines = webapp.launcher_text(sneaky).splitlines()
        self.assertEqual(1, sum(line.startswith("Exec=") for line in lines))
        self.assertIn("Name=Mouse Exec=/bin/sh", lines)

    def test_write_detect_and_remove(self) -> None:
        app = CATALOG.app("polypad-tangram")
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            self.assertNotIn("polypad-tangram", installed_apps(CATALOG, no_programs, web_dir=directory))
            path = webapp.write_launcher(app, directory)
            self.assertEqual(directory / "caramos-web-polypad-tangram.desktop", path)
            self.assertEqual(0o644, path.stat().st_mode & 0o777)
            self.assertIn("polypad-tangram", installed_apps(CATALOG, no_programs, web_dir=directory))
            self.assertTrue(webapp.remove_launcher(app, directory))
            self.assertFalse(path.exists())
            self.assertTrue(webapp.remove_launcher(app, directory))   # nothing left to remove

    def test_a_launcher_someone_else_wrote_stays(self) -> None:
        app = CATALOG.app("thecolor")
        with tempfile.TemporaryDirectory() as tmp:
            path = webapp.launcher_path(app.id, Path(tmp))
            path.write_text("[Desktop Entry]\nExec=firefox https://www.thecolor.com/\n", encoding="utf-8")
            self.assertNotIn("thecolor", installed_apps(CATALOG, no_programs, web_dir=Path(tmp)))
            self.assertFalse(webapp.remove_launcher(app, Path(tmp)))
            self.assertTrue(path.exists())

    def test_one_window_per_site(self) -> None:
        self.assertEqual("net.vietnamlinuxfamily.CaramOSWebApp.polypad_tangram", webapp.application_id("polypad-tangram"))

    def test_the_runner_is_isolated_python(self) -> None:
        self.assertTrue(RUNNER.read_text(encoding="utf-8").startswith("#!/usr/bin/python3 -I\n"))
        self.assertEqual(webapp.RUNNER, "/" + str(RUNNER.relative_to(ROOT)))


class BrowserModeTests(unittest.TestCase):
    def test_a_site_that_opens_in_the_browser_gets_no_window(self) -> None:
        from caramos_app_center import webview

        opened = []
        with mock.patch.object(webview, "find_app", return_value=CATALOG.app("polypad-tangram")), \
                mock.patch.object(webview.webapp, "open_in_browser", side_effect=lambda url: opened.append(url) or True), \
                mock.patch.object(webview.webapp, "webkit_environment", side_effect=AssertionError("WebKit started")):
            self.assertEqual(0, webview.main(["polypad-tangram"]))
        self.assertEqual(["https://polypad.amplify.com/tangram"], opened)

    def test_its_launcher_is_the_same_kind(self) -> None:
        lines = webapp.launcher_text(CATALOG.app("polypad-tangram")).splitlines()
        self.assertIn("Exec=/usr/lib/caramos-app-center/caramos-web-app polypad-tangram", lines)
        self.assertIn("Icon=tangram", lines)
        # The browser's window shows up, not one of ours: no busy cursor waiting for it.
        self.assertIn("StartupNotify=false", lines)
        self.assertFalse(any(line.startswith("StartupWMClass=") for line in lines))


class NavigationTests(unittest.TestCase):
    HOSTS = ("thecolor.com",)

    def test_pages_of_the_site_stay(self) -> None:
        for url in ("https://www.thecolor.com/Coloring/Cat.aspx", "https://thecolor.com/", "https://THECOLOR.COM./x"):
            self.assertEqual(webapp.STAY, webapp.navigation_choice(url, self.HOSTS, user_action=True), url)

    def test_links_to_other_sites_open_in_the_browser(self) -> None:
        for url in ("https://www.pinterest.com/thecolor", "http://example.org/", "mailto:info@example.org",
                    "https://thecolor.com.example.net/", "https://evil.example/?u=www.thecolor.com",
                    "http://www.thecolor.com/",    # not https: not a page of the site
                    "https://user@www.thecolor.com/"):  # a user name: the browser warns about those
            self.assertEqual(webapp.BROWSER, webapp.navigation_choice(url, self.HOSTS, user_action=True), url)

    def test_other_addresses_are_dropped(self) -> None:
        for url in ("file:///etc/passwd", "apt:vlc", "javascript:alert(1)", "intent://x"):
            self.assertEqual(webapp.DROP, webapp.navigation_choice(url, self.HOSTS, user_action=True), url)
        self.assertEqual(webapp.DROP, webapp.navigation_choice("file:///etc/passwd", self.HOSTS, user_action=False))

    def test_what_a_page_loads_by_itself_stays(self) -> None:
        # Redirects, frames and scripts of the site; ads among them are the block list's job.
        for url in ("https://api.amplify.com/x", "about:blank", "https://www.youtube.com/embed/x"):
            self.assertEqual(webapp.STAY, webapp.navigation_choice(url, self.HOSTS, user_action=False), url)

    def test_new_windows(self) -> None:
        self.assertEqual(webapp.HERE, webapp.new_window_choice("https://www.thecolor.com/a", self.HOSTS, user_action=True))
        self.assertEqual(webapp.BROWSER, webapp.new_window_choice("https://example.org/", self.HOSTS, user_action=True))
        self.assertEqual(webapp.DROP, webapp.new_window_choice("ftp://example.org/", self.HOSTS, user_action=True))
        # A pop-up: a window the page opens without a click.
        self.assertEqual(webapp.DROP, webapp.new_window_choice("https://www.thecolor.com/a", self.HOSTS, user_action=False))
        self.assertEqual(webapp.DROP, webapp.new_window_choice("https://ads.example/", self.HOSTS, user_action=False))


class RenderTests(unittest.TestCase):
    GIB = 1024 * 1024

    def test_webkit_draws_without_opengl_where_gtk_draws_with_cairo(self) -> None:
        software = {"WEBKIT_DISABLE_COMPOSITING_MODE": "1"}
        self.assertEqual(software, webapp.webkit_environment({}, set(), 8 * self.GIB))            # no GPU driver
        self.assertEqual(software, webapp.webkit_environment({}, {"virtio_gpu"}, 8 * self.GIB))
        self.assertEqual(software, webapp.webkit_environment({}, {"i915"}, 2 * self.GIB))         # 2 GB lab computer
        self.assertEqual({}, webapp.webkit_environment({}, {"i915"}, 8 * self.GIB))
        # What the user set stays.
        self.assertEqual({}, webapp.webkit_environment({"WEBKIT_DISABLE_DMABUF_RENDERER": "1"}, set(), 2 * self.GIB))
        # GSK_RENDERER is GTK 4's: it does not decide for WebKit.
        self.assertEqual(software, webapp.webkit_environment({"GSK_RENDERER": "ngl"}, set(), 2 * self.GIB))

    def test_set_before_webkit_starts(self) -> None:
        source = (ROOT / "usr/lib/python3/dist-packages/caramos_app_center/webview.py").read_text(encoding="utf-8")
        main = source[source.index("def main("):]
        self.assertLess(main.index("webapp.webkit_environment("), main.index('gi.require_version("WebKit2", "4.1")'))


class BlockListTests(unittest.TestCase):
    def setUp(self) -> None:
        self.domains = webapp.load_blocklist(DATA / "web-blocklist.txt")

    def test_the_ad_servers_of_the_sites_are_in_it(self) -> None:
        for domain in ("doubleclick.net", "googlesyndication.com", "googletagmanager.com", "pub.network", "btloader.com",
                       "amazon-adsystem.com", "quantserve.com", "adncdn.net"):
            self.assertIn(domain, self.domains)

    def test_no_site_is_blocked_by_its_own_block_list(self) -> None:
        for app in CATALOG.apps.values():
            if app.method == "web":
                self.assertEqual([], webapp.blocked_hosts_in(app.web_hosts, self.domains), app.id)
        # Nor the shared hosting services a block would take down with the ad server.
        self.assertEqual([], webapp.blocked_hosts_in(["fastly.net", "amazonaws.com", "google.com", "googleapis.com",
                                                      "github.com", "cloudflare.com"], self.domains))

    def test_rules_are_webkit_content_blocker_rules(self) -> None:
        rules = webapp.content_rules(["doubleclick.net", "pub.network"])
        self.assertEqual({"trigger": {"url-filter": "^[^:]+://+([^:/]+\\.)?doubleclick\\.net[:/]"},
                          "action": {"type": "block"}}, rules[0])
        json.dumps(rules)
        self.assertEqual(len(self.domains), len(webapp.content_rules(self.domains)))

    def test_a_bad_line_is_an_error(self) -> None:
        self.assertEqual(["a.example"], webapp.parse_blocklist("# x\n\n A.example  # y\na.example\n"))
        with self.assertRaises(ValueError):
            webapp.parse_blocklist("ads.example/path\n")


class HelperTests(unittest.TestCase):
    def run_helper(self, action: str, app_id: str, directory: Path, installed: set[str]) -> list[dict]:
        stream = io.StringIO()
        run = helper.Helper(CATALOG, DATA / "recipes", stream)
        run.launcher_dir = directory
        run.installed_debs = lambda packages: installed & set(packages)
        apt_calls = []
        run._apt_packages = lambda app, verb, packages: apt_calls.append((verb, packages))
        code = run.run(action, [app_id])
        events = [json.loads(line) for line in stream.getvalue().splitlines()]
        events.append({"apt": apt_calls, "code": code})
        return events

    def test_install_writes_the_launcher_and_remove_takes_it_away(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            events = self.run_helper("install", "thecolor", directory, {"gir1.2-webkit2-4.1"})
            self.assertEqual({"apt": [], "code": helper.EXIT_OK}, events[-1])   # WebKit is there: nothing from apt
            self.assertIn({"event": "done", "id": "thecolor", "ok": True}, events)
            self.assertTrue(webapp.launcher_path("thecolor", directory).is_file())
            events = self.run_helper("remove", "thecolor", directory, {"gir1.2-webkit2-4.1"})
            self.assertIn({"event": "done", "id": "thecolor", "ok": True}, events)
            self.assertFalse(webapp.launcher_path("thecolor", directory).exists())

    def test_install_brings_back_the_webkit_bindings(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            events = self.run_helper("install", "minimouse", Path(tmp), set())
            self.assertEqual([("install", ("gir1.2-webkit2-4.1",))], events[-1]["apt"])

    def test_remove_keeps_a_launcher_it_did_not_write(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = webapp.launcher_path("minimouse", Path(tmp))
            path.write_text("[Desktop Entry]\nExec=something-else\n", encoding="utf-8")
            events = self.run_helper("remove", "minimouse", Path(tmp), set())
            done = next(e for e in events if e.get("event") == "done")
            self.assertFalse(done["ok"])
            self.assertTrue(path.exists())


if __name__ == "__main__":
    unittest.main()
