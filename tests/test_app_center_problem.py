"""Asking VNLF for help after a failed install: the log the App Center saves and how it opens Feedback & Help."""

from __future__ import annotations

import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from caramos_app_center import problem
from caramos_app_center.catalog import load_catalog

DATA = Path(__file__).parents[1] / "usr/share/caramos-app-center"
CATALOG = load_catalog(DATA / "catalog.json", DATA / "recipes")
ZALO, ZOOM, VLC = CATALOG.app("zalo-full"), CATALOG.app("zoom"), CATALOG.app("vlc")
RESULTS = {"zalo-full": (False, "Zalo could not be installed: <urlopen error [Errno 111] Connection refused>"),
           "zoom": (True, ""), "vlc": (False, "")}
LOGS = {"zalo-full": [f"line {number}" for number in range(500)] + ["Zalo could not be installed: <urlopen error>"],
        "zoom": ["zoom output"]}


class FailureLogTests(unittest.TestCase):
    def test_only_the_apps_that_failed_with_the_end_of_their_output(self) -> None:
        text = problem.failure_log_text("install", [ZALO, ZOOM, VLC], RESULTS, LOGS, version="0.2.2")
        self.assertTrue(text.startswith("App Center 0.2.2: install failed at "))
        self.assertIn("== Zalo (zalo-full, recipe: zalo-full) ==", text)
        self.assertIn("Reason: Zalo could not be installed: <urlopen error [Errno 111] Connection refused>", text)
        self.assertNotIn("zoom output", text)
        self.assertNotIn("line 100\n", text)                       # older output is cut
        self.assertIn("line 499\n", text)
        self.assertIn("== VLC (vlc, apt: vlc) ==\nReason: not started\n(no output)", text)

    def test_ask_for_help_opens_feedback_and_help_with_the_log(self) -> None:
        with tempfile.TemporaryDirectory() as cache, \
                patch.dict(os.environ, {"XDG_CACHE_HOME": cache}), \
                patch.object(problem.subprocess, "Popen") as popen, \
                patch.object(problem, "_version", return_value="0.2.2"):
            self.assertTrue(problem.ask_for_help("install", [ZALO, ZOOM], RESULTS, LOGS, "App Center chưa cài được Zalo."))
            log = Path(cache) / "caramos-app-center/install-failure.log"
            self.assertIn("== Zalo (zalo-full", log.read_text(encoding="utf-8"))
        command = popen.call_args.args[0]
        self.assertEqual(["/usr/bin/caramos-help", "--type", "bug", "--message", "App Center chưa cài được Zalo.",
                          "--log", str(log)], command)
        self.assertTrue(popen.call_args.kwargs["start_new_session"])

    def test_nothing_to_report_when_everything_worked(self) -> None:
        with patch.object(problem.subprocess, "Popen") as popen:
            self.assertFalse(problem.ask_for_help("install", [ZOOM], RESULTS, LOGS, "message"))
        popen.assert_not_called()

    def test_the_button_needs_a_caramos_help_that_takes_logs(self) -> None:
        old = "usage: caramos-help [-h] [--type {os,bug,feedback}] [--requests] [--refresh]"
        new = old.replace("[--requests]", "[--message MESSAGE] [--log FILE] [--requests]")
        for output, expected in ((old, False), (new, True)):
            problem.help_available.cache_clear()
            with patch.object(problem.os, "access", return_value=True), \
                    patch.object(problem.subprocess, "run",
                                 return_value=problem.subprocess.CompletedProcess([], 0, output, "")):
                self.assertEqual(expected, problem.help_available())
        problem.help_available.cache_clear()
        with patch.object(problem.os, "access", return_value=False):
            self.assertFalse(problem.help_available())
        problem.help_available.cache_clear()

    def test_a_missing_help_app_is_reported(self) -> None:
        with tempfile.TemporaryDirectory() as cache, patch.dict(os.environ, {"XDG_CACHE_HOME": cache}), \
                patch.object(problem.subprocess, "Popen", side_effect=FileNotFoundError("caramos-help")), \
                patch.object(problem, "_version", return_value="0.2.2"):
            self.assertFalse(problem.ask_for_help("install", [ZALO], RESULTS, LOGS, "message"))


if __name__ == "__main__":
    unittest.main()
