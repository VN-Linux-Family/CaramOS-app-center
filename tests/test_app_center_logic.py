"""Tests for App Center detection, planning, first-run rules, events and the privileged helper."""

from __future__ import annotations

import io
import json
import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from caramos_app_center import firstrun, helper, runner
from caramos_app_center.catalog import load_catalog, parse_catalog
from caramos_app_center.detect import installed_apps, parse_dpkg_status, parse_flatpak_list, zalo_flavour
from caramos_app_center.events import emit, parse_event
from caramos_app_center.plan import FLATPAK_RUNTIME_MB, default_selection, format_size, make_plan

ROOT = Path(__file__).parents[1]
DATA = ROOT / "usr/share/caramos-app-center"
CATALOG = load_catalog(DATA / "catalog.json", DATA / "recipes")


def fake_runner(flatpaks: str = "", dpkg: str = ""):
    def run(args: list[str]) -> str:
        return flatpaks if args[0] == "flatpak" else dpkg
    return run


class DetectTests(unittest.TestCase):
    def test_parsers(self) -> None:
        self.assertEqual({"org.onlyoffice.desktopeditors"}, parse_flatpak_list("org.onlyoffice.desktopeditors\n\n"))
        self.assertEqual({"firefox", "vlc"}, parse_dpkg_status("firefox\tinstalled\nvlc:amd64\tinstalled\nzoom\tconfig-files\n"))

    def test_installed_apps_from_flatpak_dpkg_and_zalo_state(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            state = Path(tmp) / "zalo.json"
            state.write_text(json.dumps({"sha256": "x", "flavour": "full"}), encoding="utf-8")
            installed = installed_apps(
                CATALOG,
                fake_runner("org.onlyoffice.desktopeditors\n", "firefox\tinstalled\ngoogle-chrome-stable\tinstalled\n"),
                zalo_state=state,
            )
        self.assertEqual({"onlyoffice", "firefox", "google-chrome", "zalo-full"}, installed)

    def test_zalo_state_without_flavour_is_the_light_build(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            state = Path(tmp) / "zalo.json"
            state.write_text(json.dumps({"sha256": "x"}), encoding="utf-8")
            self.assertEqual("light", zalo_flavour(state))
            self.assertEqual("", zalo_flavour(Path(tmp) / "missing.json"))


class PlanTests(unittest.TestCase):
    def test_skips_installed_and_preinstalled_and_counts_the_runtime_once(self) -> None:
        plan = make_plan(CATALOG, ["onlyoffice", "brave", "firefox", "google-chrome", "vlc"], {"google-chrome"})
        self.assertEqual(["onlyoffice", "brave", "vlc"], plan.ids)
        self.assertEqual(FLATPAK_RUNTIME_MB, plan.runtime_mb)
        self.assertEqual(1300 + 420 + 80, plan.size_mb)

    def test_no_runtime_when_a_flatpak_app_is_already_installed(self) -> None:
        self.assertEqual(0, make_plan(CATALOG, ["brave"], {"libreoffice"}).runtime_mb)
        self.assertEqual(0, make_plan(CATALOG, ["vlc"], set()).runtime_mb)

    def test_default_selection_keeps_an_installed_office_suite(self) -> None:
        self.assertEqual({"onlyoffice"}, default_selection(CATALOG, set())["office"])
        self.assertEqual(set(), default_selection(CATALOG, {"wps-office"})["office"])
        self.assertEqual(set(), default_selection(CATALOG, {"google-chrome"})["browser"])

    def test_format_size_uses_the_session_decimal_mark(self) -> None:
        with patch.dict(os.environ, {"LANGUAGE": "vi_VN:vi"}):
            self.assertEqual("1,3 GB", format_size(1300))
        with patch.dict(os.environ, {"LANGUAGE": "en_US:en"}):
            self.assertEqual("1.3 GB", format_size(1300))
            self.assertEqual("2 GB", format_size(2000))
            self.assertEqual("80 MB", format_size(80))


class FirstRunTests(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.root = Path(self._tmp.name)
        self.flag = self.root / "first-run"
        self.flag.touch()
        self.cmdline = self.root / "cmdline"
        self.cmdline.write_text("BOOT_IMAGE=/vmlinuz root=UUID=x ro quiet splash\n", encoding="utf-8")
        self.home = self.root / "home"

    def should_run(self, **overrides) -> bool:
        args = {"home": self.home, "uid": 1000, "flag": self.flag, "cmdline": self.cmdline, **overrides}
        return firstrun.should_run(**args)

    def test_runs_once_per_user_on_iso_installs(self) -> None:
        self.assertTrue(self.should_run())
        firstrun.mark_done(self.home)
        self.assertFalse(self.should_run())

    def test_also_in_the_live_session_for_its_uid_999_user(self) -> None:
        self.cmdline.write_text("boot=casper locale=vi_VN.UTF-8 quiet splash\n", encoding="utf-8")
        self.assertTrue(firstrun.is_live_session(self.cmdline))
        self.assertTrue(self.should_run(uid=999))
        self.assertFalse(self.should_run(uid=998))

    def test_never_on_ota_machines_or_for_system_users(self) -> None:
        self.assertFalse(self.should_run(flag=self.root / "absent"))
        self.assertFalse(self.should_run(uid=0))
        self.assertFalse(self.should_run(uid=999))


class EventTests(unittest.TestCase):
    def test_round_trip_and_rejects_noise(self) -> None:
        stream = io.StringIO()
        emit("progress", stream, id="vlc", percent=140)
        event = parse_event(stream.getvalue())
        self.assertEqual({"event": "progress", "id": "vlc", "percent": 100}, event)
        self.assertIsNone(parse_event("Error executing command as another user"))
        self.assertIsNone(parse_event('{"event": "rm -rf"}'))
        with self.assertRaises(ValueError):
            emit("shell", stream)


class HelperArgumentTests(unittest.TestCase):
    def test_accepts_catalog_style_ids_only(self) -> None:
        self.assertEqual(("install", ["onlyoffice", "vlc"]), helper.parse_args(["install", "onlyoffice", "vlc"]))
        for bad in (["install"], ["purge", "vlc"], ["install", "../../bin/sh"], ["install", "a b"],
                    ["install", "vlc", "vlc"], ["remove", "vlc", "zoom"], ["install", *[f"app{i}" for i in range(41)]]):
            with self.assertRaises(helper.UsageError, msg=bad):
                helper.parse_args(bad)

    def test_apt_status_lines_map_to_progress(self) -> None:
        self.assertEqual(25, helper.apt_progress("dlstatus:1:50.0:Retrieving file 1 of 2"))
        self.assertEqual(75, helper.apt_progress("pmstatus:vlc:50.0:Installing vlc"))
        self.assertIsNone(helper.apt_progress("Reading package lists..."))

    def test_main_refuses_to_run_without_root(self) -> None:
        with patch.object(helper.os, "geteuid", return_value=1000):
            self.assertEqual(helper.EXIT_USAGE, helper.main(["install", "vlc"]))

    def test_runner_calls_the_fixed_helper_path_through_pkexec(self) -> None:
        with patch.dict(os.environ, {}, clear=True):
            self.assertEqual(["/usr/bin/pkexec", runner.HELPER_PATH, "install", "vlc"], runner.helper_command("install", ["vlc"]))


class HelperRunTests(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.recipes = Path(self._tmp.name)
        self.write_recipe("good", "echo PROGRESS 40\necho fetching\necho PROGRESS 100\n")
        self.write_recipe("bad", "echo boom >&2\nexit 3\n")
        data = json.loads((DATA / "catalog.json").read_text(encoding="utf-8"))
        base = next(app for app in data["apps"] if app["id"] == "zoom")
        apps = []
        for app_id, recipe in (("good-app", "good"), ("bad-app", "bad")):
            app = dict(base, id=app_id, install={"recipe": recipe}, detect={"deb": app_id})
            apps.append(app)
        apps.append(next(app for app in data["apps"] if app["id"] == "firefox"))
        self.catalog = parse_catalog({"schema": 1, "apps": apps, "categories": data["categories"], "steps": []},
                                     self.recipes)

    def write_recipe(self, name: str, body: str) -> None:
        path = self.recipes / name
        path.write_text("#!/bin/sh\n" + body, encoding="utf-8")
        path.chmod(0o755)

    def run_helper(self, ids: list[str]) -> tuple[int, list[dict]]:
        stream = io.StringIO()
        code = helper.Helper(self.catalog, self.recipes, stream).run("install", ids)
        return code, [json.loads(line) for line in stream.getvalue().splitlines()]

    def test_streams_progress_and_logs_and_carries_on_after_a_failure(self) -> None:
        code, events = self.run_helper(["bad-app", "good-app"])
        self.assertEqual(helper.EXIT_FAILED, code)
        kinds = [(e["event"], e.get("id")) for e in events]
        self.assertIn(("log", "bad-app"), kinds)
        self.assertIn({"event": "done", "id": "bad-app", "ok": False, "error": "bad install exited with 3"}, events)
        self.assertIn({"event": "progress", "id": "good-app", "percent": 40, "message": ""}, events)
        self.assertIn({"event": "log", "id": "good-app", "line": "fetching"}, events)
        self.assertIn({"event": "done", "id": "good-app", "ok": True}, events)
        self.assertEqual({"event": "finished", "ok": False, "failed": ["bad-app"]}, events[-1])

    def test_unknown_and_preinstalled_apps_are_reported_not_run(self) -> None:
        code, events = self.run_helper(["firefox", "nope"])
        self.assertEqual(helper.EXIT_FAILED, code)
        self.assertIn({"event": "done", "id": "nope", "ok": False, "error": "unknown app"}, events)
        firefox = next(e for e in events if e["event"] == "done" and e["id"] == "firefox")
        self.assertFalse(firefox["ok"])

    def test_flatpak_apps_use_the_system_installation(self) -> None:
        catalog = CATALOG
        stream = io.StringIO()
        calls = []

        def fake_install(app_id, on_progress, on_log):
            calls.append(app_id)
            on_progress(50, "Downloading")
            on_progress(100, "")

        with patch.object(helper.flatpak_ops, "install", side_effect=fake_install):
            code = helper.Helper(catalog, DATA / "recipes", stream).run("install", ["onlyoffice"])
        self.assertEqual(helper.EXIT_OK, code)
        self.assertEqual(["org.onlyoffice.desktopeditors"], calls)
        self.assertIn('"percent": 50', stream.getvalue())


if __name__ == "__main__":
    unittest.main()
