"""Install links from the VNLF app site (caramos-app://install/<Flathub app id>): parsing, the catalog's own cards,
what Flathub says about other apps, and the helper's Flathub install with Flatpak or Flathub set up first."""

from __future__ import annotations

import base64
import configparser
import io
import json
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from caramos_app_center import flatpak_ops, helper, links
from caramos_app_center.catalog import CatalogError, load_catalog, parse_catalog
from caramos_app_center.flathub import FlathubError, fetch_app, fetch_icon, icon_url_ok

ROOT = Path(__file__).parents[1]
DATA = ROOT / "usr/share/caramos-app-center"
CATALOG = load_catalog(DATA / "catalog.json", DATA / "recipes")
API = "https://flathub.org/api/v2"
PNG = b"\x89PNG\r\n\x1a\n" + b"\0" * 64


class LinkTests(unittest.TestCase):
    def test_install_links(self) -> None:
        for text, app_id in (("caramos-app://install/org.gimp.GIMP", "org.gimp.GIMP"),
                             ("caramos-app://install/org.gimp.GIMP/", "org.gimp.GIMP"),
                             ("CARAMOS-APP://Install/org.gnome.Calculator", "org.gnome.Calculator"),
                             ("caramos-app://install/com.github.tchx84.Flatseal", "com.github.tchx84.Flatseal"),
                             ("caramos-app://install/io.github.some_one.App-Name", "io.github.some_one.App-Name")):
            self.assertEqual(app_id, links.parse_link(text).app_id, text)

    def test_anything_else_is_refused(self) -> None:
        for text in ("", "org.gimp.GIMP", "https://apps.vietnamlinuxfamily.net/app/gimp", "caramos-app://install/",
                     "caramos-app://remove/org.gimp.GIMP", "caramos-app:install/org.gimp.GIMP",
                     "caramos-app://install/org.gimp", "caramos-app://install/org..GIMP",
                     "caramos-app://install/org.1gimp.GIMP", "caramos-app://install/org.gi-mp.GIMP",
                     "caramos-app://install/.org.gimp.GIMP", "caramos-app://install/org.gimp.GIMP?x=1",
                     "caramos-app://install/org.gimp.GIMP#top", "caramos-app://install/org.gimp.GIMP/extra",
                     "caramos-app://install/org.gimp.GIMP//", "caramos-app://install/org.gimp%2EGIMP",
                     "caramos-app://install/org.gimp.GIMP;rm", "caramos-app://install/org.gimp.$(id)",
                     "caramos-app://install/../org.gimp.GIMP", "caramos-app://install/org.gimp.GIMP\n",
                     "caramos-app://install/org.gimp.GIMP ", "caramos-app://install/org.gímp.GIMP",
                     "caramos-app://user@install/org.gimp.GIMP", "caramos-app://install/--help.x.y",
                     "caramos-app://install/" + "a." * 150 + "b", None, 42):
            with self.assertRaises(links.LinkError, msg=repr(text)):
                links.parse_link(text)


class CatalogCardTests(unittest.TestCase):
    def test_links_to_suggested_apps_open_their_own_card(self) -> None:
        self.assertEqual("gimp", CATALOG.app_for_flathub_id("org.gimp.GIMP").id)            # from Flathub
        self.assertEqual("vlc", CATALOG.app_for_flathub_id("org.videolan.VLC").id)          # Ubuntu's VLC
        self.assertEqual("google-chrome", CATALOG.app_for_flathub_id("com.google.Chrome").id)  # Google's package
        self.assertEqual("firefox", CATALOG.app_for_flathub_id("org.mozilla.firefox").id)   # comes with CaramOS
        self.assertEqual("klickety", CATALOG.app_for_flathub_id("org.kde.klickety").id)
        self.assertIsNone(CATALOG.app_for_flathub_id("org.gnome.Calculator"))
        self.assertIsNone(CATALOG.app_for_flathub_id(""))

    def test_every_flathub_id_is_valid_and_names_one_app(self) -> None:
        ids = [app.flatpak_id or app.flathub_alias for app in CATALOG.apps.values()
               if app.flatpak_id or app.flathub_alias]
        self.assertEqual(len(ids), len(set(ids)))
        for app_id in ids:
            self.assertTrue(links.is_flathub_id(app_id), app_id)

    def test_validation(self) -> None:
        data = json.loads((DATA / "catalog.json").read_text(encoding="utf-8"))

        def broken(mutate) -> None:
            copy = json.loads(json.dumps(data))
            mutate({app["id"]: app for app in copy["apps"]})
            with self.assertRaises(CatalogError):
                parse_catalog(copy, DATA / "recipes")

        broken(lambda apps: apps["gimp"].update(flathub="org.gimp.GIMP"))         # a Flathub app has its own id
        broken(lambda apps: apps["vlc"].update(flathub="org.gimp.GIMP"))          # two apps for one id
        broken(lambda apps: apps["vlc"].update(flathub="not an id"))


def fake_api(answers: dict[str, object]):
    def get_json(url: str) -> object:
        answer = answers.get(url)
        if isinstance(answer, Exception):
            raise answer
        if answer is None:
            raise FlathubError("not-found", url)
        return answer
    return get_json


CALCULATOR = {
    f"{API}/appstream/org.gnome.Calculator": {
        "id": "org.gnome.Calculator", "type": "desktop-application", "name": "Calculator",
        "summary": "Perform arithmetic, scientific or financial calculations", "developer_name": "The GNOME Project",
        "project_license": "GPL-3.0-or-later", "is_free_license": True, "is_eol": False,
        "icon": "https://dl.flathub.org/media/org/gnome/Calculator/x/icons/128x128/org.gnome.Calculator.png",
        "urls": {"homepage": "https://apps.gnome.org/Calculator"}},
    f"{API}/summary/org.gnome.Calculator": {"download_size": 1955737, "installed_size": 5081088,
                                            "metadata": {"runtime": "org.gnome.Platform/x86_64/51"}},
    f"{API}/verification/org.gnome.Calculator/status": {"verified": True},
}


class FlathubApiTests(unittest.TestCase):
    def test_what_flathub_says_about_an_app(self) -> None:
        app = fetch_app("org.gnome.Calculator", get_json=fake_api(CALCULATOR))
        self.assertEqual(("Calculator", "The GNOME Project", "GPL-3.0-or-later", True, True),
                         (app.name, app.developer, app.licence, app.free, app.verified))
        self.assertEqual((1955737, 5081088, "org.gnome.Platform/x86_64/51"),
                         (app.download_size, app.installed_size, app.runtime))
        self.assertTrue(app.icon_url.startswith("https://dl.flathub.org/"))
        self.assertEqual("https://apps.gnome.org/Calculator", app.homepage)

    def test_a_missing_verification_is_not_an_error(self) -> None:
        answers = dict(CALCULATOR)
        answers[f"{API}/verification/org.gnome.Calculator/status"] = FlathubError("unavailable")
        self.assertFalse(fetch_app("org.gnome.Calculator", get_json=fake_api(answers)).verified)

    def test_proprietary_app(self) -> None:
        answers = {key.replace("org.gnome.Calculator", "com.example.Closed.App"): value
                   for key, value in CALCULATOR.items()}
        answers[f"{API}/appstream/com.example.Closed.App"] = dict(
            CALCULATOR[f"{API}/appstream/org.gnome.Calculator"], id="com.example.Closed.App",
            project_license="LicenseRef-proprietary=https://example.com/eula", is_free_license=False,
            icon="http://example.com/icon.png", urls={"homepage": "javascript:alert(1)"})
        app = fetch_app("com.example.Closed.App", get_json=fake_api(answers))
        self.assertFalse(app.free)
        self.assertEqual("", app.icon_url, "icons only from dl.flathub.org over https")
        self.assertEqual("", app.homepage)

    def test_not_found_runtimes_and_network(self) -> None:
        with self.assertRaises(FlathubError) as caught:
            fetch_app("org.example.Missing", get_json=fake_api({}))
        self.assertEqual("not-found", caught.exception.kind)
        runtime = {f"{API}/appstream/org.gnome.Platform": {"id": "org.gnome.Platform", "type": "runtime"}}
        with self.assertRaises(FlathubError) as caught:
            fetch_app("org.gnome.Platform", get_json=fake_api(runtime))
        self.assertEqual("not-found", caught.exception.kind)
        renamed = {f"{API}/appstream/org.old.Name": {"id": "org.new.Name", "type": "desktop-application"}}
        with self.assertRaises(FlathubError):
            fetch_app("org.old.Name", get_json=fake_api(renamed))
        offline = {f"{API}/appstream/org.gnome.Calculator": FlathubError("network", "Name or service not known")}
        with self.assertRaises(FlathubError) as caught:
            fetch_app("org.gnome.Calculator", get_json=fake_api(offline))
        self.assertEqual("network", caught.exception.kind)
        with self.assertRaises(FlathubError):
            fetch_app("../../etc", get_json=fake_api(CALCULATOR))

    def test_icons(self) -> None:
        self.assertTrue(icon_url_ok("https://dl.flathub.org/media/a.png"))
        for url in ("http://dl.flathub.org/media/a.png", "https://dl.flathub.org.example.net/a.png",
                    "https://user@dl.flathub.org/a.png", "https://dl.flathub.org:8443/a.png", "file:///etc/passwd"):
            self.assertFalse(icon_url_ok(url), url)
        app = fetch_app("org.gnome.Calculator", get_json=fake_api(CALCULATOR))

        class Response(io.BytesIO):
            def __enter__(self):
                return self

            def __exit__(self, *_exc) -> None:
                self.close()

        with tempfile.TemporaryDirectory() as tmp:
            path = fetch_icon(app, Path(tmp), opener=lambda *_a, **_k: Response(PNG))
            self.assertEqual(Path(tmp) / "org.gnome.Calculator.png", path)
            self.assertEqual(PNG, path.read_bytes())
            self.assertIsNone(fetch_icon(app, Path(tmp) / "other", opener=lambda *_a, **_k: Response(b"<svg/>")))
            self.assertIsNone(fetch_icon(app, Path(tmp) / "big",
                                         opener=lambda *_a, **_k: Response(PNG + b"\0" * 2_000_000)))


class Done:
    def __init__(self, stdout: str = "", returncode: int = 0) -> None:
        self.stdout, self.returncode = stdout, returncode


class FlathubSetupTests(unittest.TestCase):
    def test_state_of_flatpak_and_flathub(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            flatpak = Path(tmp) / "flatpak"
            self.assertEqual(flatpak_ops.NO_FLATPAK, flatpak_ops.flathub_state(lambda args: Done(), flatpak))
            flatpak.touch()
            self.assertEqual(flatpak_ops.NO_REMOTE, flatpak_ops.flathub_state(lambda args: Done(""), flatpak))
            self.assertEqual(flatpak_ops.REMOTE_DISABLED,
                             flatpak_ops.flathub_state(lambda args: Done("flathub\tsystem,disabled\n"), flatpak))
            self.assertEqual(flatpak_ops.READY, flatpak_ops.flathub_state(lambda args: Done("flathub\tsystem\n"), flatpak))
            self.assertEqual(flatpak_ops.NO_REMOTE,
                             flatpak_ops.flathub_state(lambda args: Done("flathub-beta\tsystem\n"), flatpak))

    def test_setup_commands(self) -> None:
        self.assertEqual([["/usr/bin/flatpak", "remote-add", "--system", "--if-not-exists", "flathub",
                           "/usr/share/caramos-app-center/flathub.flatpakrepo"]],
                         flatpak_ops.flathub_setup_commands(flatpak_ops.NO_REMOTE))
        self.assertEqual([["/usr/bin/flatpak", "remote-modify", "--system", "--enable", "flathub"]],
                         flatpak_ops.flathub_setup_commands(flatpak_ops.REMOTE_DISABLED))
        self.assertEqual([], flatpak_ops.flathub_setup_commands(flatpak_ops.READY))

    def test_the_shipped_flathub_remote(self) -> None:
        config = configparser.ConfigParser()
        config.read(DATA / "flathub.flatpakrepo")
        repo = config["Flatpak Repo"]
        self.assertEqual("https://dl.flathub.org/repo/", repo["Url"])
        if shutil.which("gpg") is None:
            self.skipTest("needs gpg")
        with tempfile.TemporaryDirectory() as home:
            key = Path(home) / "flathub.gpg"
            key.write_bytes(base64.b64decode(repo["GPGKey"]))
            listing = subprocess.run(["gpg", "--homedir", home, "--batch", "--with-colons", "--show-keys", str(key)],
                                     check=True, capture_output=True, text=True).stdout
        fingerprints = [line.split(":")[9] for line in listing.splitlines() if line.startswith("fpr:")]
        self.assertEqual("6E5C05D979C76DAF93C081354184DD4D907A7CAE", fingerprints[0])


class HelperFlathubTests(unittest.TestCase):
    def test_arguments(self) -> None:
        self.assertEqual(("flathub-install", ["org.gnome.Calculator"]),
                         helper.parse_args(["flathub-install", "org.gnome.Calculator"]))
        for bad in (["flathub-install"], ["flathub-install", "org.gnome.Calculator", "org.gimp.GIMP"],
                    ["flathub-install", "--user"], ["flathub-install", "../x.y.z"], ["flathub-remove", "gimp"],
                    ["flathub-update", "org.gnome.Calculator"]):
            with self.assertRaises(helper.UsageError, msg=bad):
                helper.parse_args(bad)

    def run_flathub(self, action: str, states: list[str]) -> tuple[int, list[dict], list, list, list]:
        stream = io.StringIO()
        run = helper.Helper(CATALOG, DATA / "recipes", stream)
        states = list(states)
        run.flathub_state = lambda: states.pop(0) if len(states) > 1 else states[0]
        apt_calls, commands, installs = [], [], []
        run._apt_packages = lambda app, verb, packages: apt_calls.append((verb, packages))

        def fake_stream(args, on_line, **_kwargs):
            commands.append(args)
            return 0

        def fake_flatpak(app_id, on_progress, on_log):
            installs.append(app_id)
            on_progress(100, "")

        with patch.object(helper, "_stream", side_effect=fake_stream), \
                patch.object(helper.flatpak_ops, "install", side_effect=fake_flatpak), \
                patch.object(helper.flatpak_ops, "uninstall", side_effect=fake_flatpak):
            code = run.run_flathub(action, "org.gnome.Calculator")
        events = [json.loads(line) for line in stream.getvalue().splitlines()]
        return code, events, apt_calls, commands, installs

    def test_install_from_flathub(self) -> None:
        code, events, apt_calls, commands, installs = self.run_flathub("flathub-install", [flatpak_ops.READY])
        self.assertEqual(helper.EXIT_OK, code)
        self.assertEqual(([], [], ["org.gnome.Calculator"]), (apt_calls, commands, installs))
        self.assertIn({"event": "done", "id": "org.gnome.Calculator", "ok": True}, events)
        self.assertEqual({"event": "finished", "ok": True, "failed": []}, events[-1])

    def test_flathub_is_added_or_turned_back_on_first(self) -> None:
        _code, _events, _apt, commands, installs = self.run_flathub("flathub-install", [flatpak_ops.NO_REMOTE])
        self.assertEqual([["/usr/bin/flatpak", "remote-add", "--system", "--if-not-exists", "flathub",
                           "/usr/share/caramos-app-center/flathub.flatpakrepo"]], commands)
        self.assertEqual(["org.gnome.Calculator"], installs)
        _code, _events, _apt, commands, _installs = self.run_flathub("flathub-install", [flatpak_ops.REMOTE_DISABLED])
        self.assertEqual([["/usr/bin/flatpak", "remote-modify", "--system", "--enable", "flathub"]], commands)

    def test_flatpak_is_installed_first(self) -> None:
        _code, events, apt_calls, commands, installs = self.run_flathub(
            "flathub-install", [flatpak_ops.NO_FLATPAK, flatpak_ops.NO_REMOTE])
        self.assertEqual([("install", ("flatpak", "gir1.2-flatpak-1.0"))], apt_calls)
        self.assertEqual("remote-add", commands[0][1])
        self.assertEqual(["org.gnome.Calculator"], installs)
        logs = [event["line"] for event in events if event["event"] == "log"]
        self.assertTrue(any("installing it first" in line for line in logs))

    def test_flatpak_that_cannot_be_installed_stops_there(self) -> None:
        code, events, _apt, _commands, installs = self.run_flathub("flathub-install", [flatpak_ops.NO_FLATPAK])
        self.assertEqual(helper.EXIT_FAILED, code)
        self.assertEqual([], installs)
        done = next(event for event in events if event["event"] == "done")
        self.assertEqual("Flatpak could not be installed", done["error"])

    def test_remove_sets_nothing_up(self) -> None:
        code, _events, apt_calls, commands, installs = self.run_flathub("flathub-remove", [flatpak_ops.NO_REMOTE])
        self.assertEqual(helper.EXIT_OK, code)
        self.assertEqual(([], [], ["org.gnome.Calculator"]), (apt_calls, commands, installs))

    def test_catalog_flatpak_apps_get_flathub_too(self) -> None:
        stream = io.StringIO()
        run = helper.Helper(CATALOG, DATA / "recipes", stream)
        run.flathub_state = lambda: flatpak_ops.NO_REMOTE
        commands = []
        with patch.object(helper, "_stream", side_effect=lambda args, *_a, **_k: commands.append(args) or 0), \
                patch.object(helper.flatpak_ops, "install", side_effect=lambda *_a: None):
            self.assertEqual(helper.EXIT_OK, run.run("install", ["gimp"]))
        self.assertEqual("remote-add", commands[0][1])


class LauncherTests(unittest.TestCase):
    def test_the_link_handler(self) -> None:
        desktop = (ROOT / "usr/share/applications/caramos-app-center-link.desktop").read_text(encoding="utf-8")
        lines = desktop.splitlines()
        self.assertIn("MimeType=x-scheme-handler/caramos-app;", lines)
        self.assertIn("NoDisplay=true", lines)
        self.assertIn("Exec=/usr/bin/caramos-app-center %u", lines)
        install = (ROOT / "debian/install").read_text(encoding="utf-8")
        self.assertIn("usr/share/applications/caramos-app-center-link.desktop usr/share/applications/", install)

    def test_links_reach_the_running_app_center(self) -> None:
        source = (ROOT / "usr/lib/python3/dist-packages/caramos_app_center/ui/app.py").read_text(encoding="utf-8")
        self.assertIn('Gio.SimpleAction.new("open-link", GLib.VariantType.new("s"))', source)
        self.assertIn('application.activate_action("open-link", GLib.Variant("s", args.link))', source)


@unittest.skipUnless(os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY"), "needs a display")
class LinkPageTests(unittest.TestCase):
    """The not-in-catalog flow with a fake Flathub API and the fake helper: the page, Install, the installed page."""

    application = None

    @classmethod
    def setUpClass(cls) -> None:
        import gi

        gi.require_version("Gtk", "4.0")
        gi.require_version("Adw", "1")
        from gi.repository import Adw, Gio, GLib, Gtk

        if not Gtk.init_check():
            raise unittest.SkipTest("no display")
        Adw.init()
        cls.GLib, cls.Gtk = GLib, Gtk
        # One application for the class: a second one with the same id cannot be exported on the bus.
        cls.application = Adw.Application(application_id=f"net.vietnamlinuxfamily.CaramOSAppCenterTest{os.getpid()}",
                                          flags=Gio.ApplicationFlags.NON_UNIQUE)
        cls.application.register(None)

    def labels(self, widget) -> list[str]:
        found = []
        child = widget.get_first_child()
        while child is not None:
            if isinstance(child, self.Gtk.Label):
                found.append(child.get_label())
            found += self.labels(child)
            child = child.get_next_sibling()
        return found

    def buttons(self, widget) -> dict[str, object]:
        found = {}
        child = widget.get_first_child()
        while child is not None:
            if isinstance(child, self.Gtk.Button) and child.get_label():
                found[child.get_label()] = child
            found.update(self.buttons(child))
            child = child.get_next_sibling()
        return found

    def wait_for(self, condition, seconds: float = 20.0) -> None:
        import time

        context = self.GLib.MainContext.default()
        deadline = time.monotonic() + seconds
        while not condition():
            if time.monotonic() > deadline:
                self.fail("timed out")
            context.iteration(False)
            time.sleep(0.01)

    def test_a_flathub_app_from_a_link(self) -> None:
        from caramos_app_center.flathub import fetch_app as real_fetch
        from caramos_app_center.ui import window as window_module

        installed = {"now": False}
        env = {"CARAMOS_APP_CENTER_FAKE_HELPER": "1", "CARAMOS_APP_CENTER_FAKE_DELAY": "0", "LANGUAGE": "en_US:en",
               "PYTHONPATH": str(ROOT / "usr/lib/python3/dist-packages")}
        with patch.dict(os.environ, env), \
                patch.object(window_module, "fetch_app", lambda app_id: real_fetch(app_id, get_json=fake_api(CALCULATOR))), \
                patch.object(window_module, "fetch_icon", lambda *_a, **_k: None), \
                patch.object(window_module, "flatpak_app_installed", lambda app_id: installed["now"]), \
                patch.object(window_module, "flatpak_runtime_installed", lambda runtime: False), \
                patch.object(window_module.flatpak_ops, "flathub_state", lambda: flatpak_ops.NO_REMOTE), \
                patch.object(window_module.MainWindow, "_detect_installed", lambda self: None):
            window = window_module.MainWindow(self.application, CATALOG, store=False, first_run=False,
                                              link="caramos-app://install/org.gnome.Calculator")
            window._on_detected(set())
            self.wait_for(lambda: "An app from Flathub" in self.labels(window))
            texts = self.labels(window)
            self.assertIn("Calculator", texts)
            self.assertIn("The GNOME Project. Verified by Flathub.", texts)
            self.assertIn("Open source (GPL-3.0-or-later).", texts)
            self.assertTrue(any(text.startswith("This computer does not use Flathub yet.") for text in texts))
            buttons = self.buttons(window)
            self.assertIn("Later", buttons)
            installed["now"] = True
            buttons["Install"].emit("clicked")
            self.wait_for(lambda: "Installed on this computer." in self.labels(window))
            self.assertIn("Remove", self.buttons(window))
            window.destroy()

    def test_bad_links_and_catalog_cards(self) -> None:
        from caramos_app_center.ui import window as window_module

        with patch.dict(os.environ, {"LANGUAGE": "en_US:en"}), \
                patch.object(window_module.MainWindow, "_detect_installed", lambda self: None):
            window = window_module.MainWindow(self.application, CATALOG, store=False, first_run=False,
                                              link="caramos-app://install/org.gimp.GIMP;reboot")
            window._on_detected(set())
            self.assertIn("This link cannot be opened", self.labels(window))
            window.open_link("caramos-app://install/org.videolan.VLC")
            texts = self.labels(window)
            self.assertIn("VLC", texts)
            self.assertIn("Install", self.buttons(window))
            window.destroy()


if __name__ == "__main__":
    unittest.main()
