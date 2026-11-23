"""Installs that used to fail for reasons outside the app (tickets H-1091, H-1094, H-1096):

- a package dpkg could not finish (an NVIDIA driver whose module does not build) fails every apt-get install,
  though the app itself gets installed: the app counts as installed, otherwise the broken package is named;
- the CaramOS installer on the live USB keeps debconf locked: an install that fails for it waits until it is
  free and tries once more, or says so; other installs do not wait;
- a source that no longer works fails apt-get update: the install goes on with the lists there are;
- a download cut in the middle goes on from where it stopped.
"""

from __future__ import annotations

import io
import json
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from caramos_app_center import helper, problem
from caramos_app_center.catalog import parse_catalog

ROOT = Path(__file__).parents[1]
DATA = ROOT / "usr/share/caramos-app-center"
DPKG_FAILURE = """Setting up vlc (3.0.20-3build6) ...
Errors were encountered while processing:
 nvidia-dkms-470
 nvidia-driver-470
E: Sub-process /usr/bin/dpkg returned an error code (1)
"""


class HelperTests(unittest.TestCase):
    def setUp(self) -> None:
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        self.recipes = Path(folder.name)
        self.output = self.recipes / "output.txt"
        self.output.write_text(DPKG_FAILURE, encoding="utf-8")
        self.write_recipe("broken-dpkg", f"cat '{self.output}'\nexit 100\n")
        self.write_recipe("good", "echo PROGRESS 100\n")
        # Fails the first time as ttf-mscorefonts-installer did next to the running installer (H-1091).
        marker = self.recipes / "tried"
        self.write_recipe("fonts", f"""if [ ! -e '{marker}' ]; then
    touch '{marker}'
    echo 'debconf: DbDriver "config": /var/cache/debconf/config.dat is locked by another process: Resource temporarily unavailable'
    exit 1
fi
echo PROGRESS 100
""")
        data = json.loads((DATA / "catalog.json").read_text(encoding="utf-8"))
        base = next(app for app in data["apps"] if app["id"] == "zoom")
        apps = [dict(base, id="chrome-test", install={"recipe": "broken-dpkg"}, detect={"deb": "chrome-test"}),
                dict(base, id="good-app", install={"recipe": "good"}, detect={"deb": "good-app"}),
                dict(base, id="fonts-test", install={"recipe": "fonts"}, detect={"deb": "fonts-test"}),
                dict(base, id="vlc-test", install={"apt": ["vlc"]}, detect={"deb": "vlc"})]
        self.catalog = parse_catalog({"schema": 1, "apps": apps, "categories": data["categories"], "steps": []},
                                     self.recipes)

    def write_recipe(self, name: str, body: str) -> None:
        path = self.recipes / name
        path.write_text("#!/bin/sh\n" + body, encoding="utf-8")
        path.chmod(0o755)

    def run_helper(self, ids: list[str], *, installed: set[str] = frozenset(), locked=lambda: False,
                   wait: float = 0) -> tuple[int, list[dict]]:
        stream = io.StringIO()
        runner = helper.Helper(self.catalog, self.recipes, stream)
        runner.installed_apps = lambda catalog: set(installed)
        runner.debconf_locked = locked
        runner.debconf_wait = wait
        with patch.object(helper, "DEBCONF_POLL", 0.01):
            code = runner.run("install", ids)
        return code, [json.loads(line) for line in stream.getvalue().splitlines()]

    def done(self, events: list[dict], app_id: str) -> dict:
        return next(event for event in events if event["event"] == "done" and event["id"] == app_id)

    def test_an_app_installed_despite_another_broken_package_counts_as_installed(self) -> None:
        code, events = self.run_helper(["chrome-test"], installed={"chrome-test"})
        self.assertEqual(helper.EXIT_OK, code)
        self.assertEqual({"event": "done", "id": "chrome-test", "ok": True}, self.done(events, "chrome-test"))
        self.assertIn("dpkg could not finish other packages: nvidia-dkms-470, nvidia-driver-470",
                      " ".join(event.get("line", "") for event in events))

    def test_otherwise_the_broken_packages_are_named(self) -> None:
        code, events = self.run_helper(["chrome-test"])
        self.assertEqual(helper.EXIT_FAILED, code)
        done = self.done(events, "chrome-test")
        self.assertEqual(("broken-packages", ["nvidia-dkms-470", "nvidia-driver-470"]), (done["code"], done["packages"]))
        self.assertIn("Driver Check", problem.failure_text(done))
        self.assertIn("nvidia-dkms-470, nvidia-driver-470", problem.failure_text(done))

    def log_text(self, events: list[dict]) -> str:
        return " ".join(event.get("line", "") for event in events)

    def test_an_install_debconf_was_locked_for_waits_and_tries_again(self) -> None:
        states = iter([True, True, False])
        code, events = self.run_helper(["fonts-test"], locked=lambda: next(states, False), wait=60)
        self.assertEqual(helper.EXIT_OK, code)
        self.assertTrue(self.done(events, "fonts-test")["ok"])
        self.assertIn("Waiting for another program", self.log_text(events))
        self.assertIn("trying once more", self.log_text(events))

    def test_an_installer_that_keeps_debconf_locked_is_reported(self) -> None:
        code, events = self.run_helper(["fonts-test"], locked=lambda: True, wait=0)
        self.assertEqual(helper.EXIT_FAILED, code)
        done = self.done(events, "fonts-test")
        self.assertEqual("busy", done["code"])
        self.assertIn("the CaramOS installer", problem.failure_text(done))

    def test_other_installs_do_not_wait_for_debconf(self) -> None:
        code, events = self.run_helper(["good-app"], locked=lambda: True, wait=0)
        self.assertEqual(helper.EXIT_OK, code)
        self.assertNotIn("Waiting for another program", self.log_text(events))
        code, events = self.run_helper(["chrome-test"], locked=lambda: True, wait=0)
        self.assertEqual("broken-packages", self.done(events, "chrome-test")["code"])

    def test_a_failed_apt_get_update_does_not_stop_the_install(self) -> None:
        calls = []

        def fake_stream(args, on_line, *, env=None, timeout=helper.RECIPE_TIMEOUT):
            calls.append(args)
            if "update" in args:
                on_line("E: The repository 'cdrom://Linux Mint 22.3 _Zena_ - Release amd64 20260108 noble Release' "
                        "does not have a Release file.")
                return 100
            return 0

        with patch.object(helper, "_stream", side_effect=fake_stream):
            code, events = self.run_helper(["vlc-test"])
        self.assertEqual(helper.EXIT_OK, code)
        self.assertEqual(2, len(calls))
        self.assertTrue(self.done(events, "vlc-test")["ok"])


    def test_dpkg_asks_nothing_about_configuration_files(self) -> None:
        """The helper has no terminal: at a configuration file question dpkg read end-of-file and failed (H-1207)."""

        calls = []

        def fake_stream(args, on_line, *, env=None, timeout=helper.RECIPE_TIMEOUT):
            calls.append((args, env))
            return 0

        with patch.object(helper, "_stream", side_effect=fake_stream):
            code, _events = self.run_helper(["vlc-test"])
        self.assertEqual(helper.EXIT_OK, code)
        args, env = next(call for call in calls if "install" in call[0])
        options = args[:args.index("install")]
        for option in ("Dpkg::Options::=--force-confdef", "Dpkg::Options::=--force-confold"):
            self.assertEqual("-o", options[options.index(option) - 1])
        self.assertEqual("noninteractive", env["DEBIAN_FRONTEND"])
        common = (DATA / "recipes/lib/common.sh").read_text(encoding="utf-8")
        self.assertIn("-o Dpkg::Options::=--force-confdef -o Dpkg::Options::=--force-confold", common)

class DpkgErrorsTests(unittest.TestCase):
    def test_reads_the_packages_dpkg_could_not_configure(self) -> None:
        errors = helper.DpkgErrors()
        for line in DPKG_FAILURE.splitlines():
            errors.feed(line)
        self.assertEqual(["nvidia-dkms-470", "nvidia-driver-470"], errors.packages)

    def test_the_lock_check_follows_flock(self) -> None:
        with tempfile.NamedTemporaryFile() as handle:
            path = Path(handle.name)
            self.assertFalse(helper.debconf_locked(path))
            import fcntl
            with path.open("rb") as other:
                fcntl.flock(other, fcntl.LOCK_EX)
                self.assertTrue(helper.debconf_locked(path))
            self.assertFalse(helper.debconf_locked(path))
        self.assertFalse(helper.debconf_locked(Path("/nonexistent/config.dat")))


class FailureTextTests(unittest.TestCase):
    def test_codes_and_plain_errors(self) -> None:
        self.assertEqual("curl: (56) Recv failure", problem.failure_text({"error": "curl: (56) Recv failure"}))
        text = problem.failure_text({"code": "broken-packages", "packages": ["libfoo"], "error": "x"})
        self.assertIn("libfoo", text)
        self.assertNotIn("Driver Check", text)


@unittest.skipUnless(shutil.which("bash"), "bash is needed")
class DownloadTests(unittest.TestCase):
    """common.sh download() with a fake curl that is cut off twice."""

    def test_a_cut_download_goes_on(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            tools = root / "bin"
            tools.mkdir()
            (tools / "curl").write_text("""#!/bin/sh
case " $* " in *" -fsIL "*) printf 'HTTP/1.1 200 OK\\r\\ncontent-length: 4\\r\\n\\r\\n'; exit 0 ;; esac
count=$(cat "$COUNT" 2>/dev/null || echo 0); count=$((count + 1)); echo "$count" > "$COUNT"
while [ "$#" -gt 0 ]; do [ "$1" = -o ] && out="$2"; shift; done
if [ "$count" -le 2 ]; then printf 'da' >> "$out"; echo "curl: (56) Recv failure: Connection reset by peer" >&2; exit 56; fi
printf 'ta' >> "$out"
""", encoding="utf-8")
            (tools / "curl").chmod(0o755)
            (tools / "sleep").write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
            (tools / "sleep").chmod(0o755)
            script = (f". '{DATA}/recipes/lib/common.sh'\nexport PATH='{tools}':\"$PATH\"\n"
                      f"download https://example.invalid/file '{root}/file'\n")
            result = subprocess.run(["bash", "-c", script], capture_output=True, text=True, timeout=60,
                                    env={**os.environ, "COUNT": str(root / "count")})
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertEqual("3", (root / "count").read_text(encoding="utf-8").strip())
            self.assertIn("trying again (2/5)", result.stdout)
            self.assertEqual("dadata", (root / "file").read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
