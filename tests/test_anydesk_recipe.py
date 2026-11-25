"""The AnyDesk recipe: AnyDesk's repository with AnyDesk's key only, and nothing left behind when it fails or is
removed."""

from __future__ import annotations

import contextlib
import importlib.machinery
import importlib.util
import io
import shutil
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).parents[1]
RECIPE = ROOT / "usr/share/caramos-app-center/recipes/anydesk"
_loader = importlib.machinery.SourceFileLoader("anydesk_recipe", str(RECIPE))
recipe = importlib.util.module_from_spec(importlib.util.spec_from_loader(_loader.name, _loader))
_loader.exec_module(recipe)

# https://keys.anydesk.com/repos/DEB-GPG-KEY on 2026-11-25.
ANYDESK_KEY = (ROOT / "tests/data/anydesk-DEB-GPG-KEY.asc").read_bytes()
OTHER_KEY = (ROOT / "keys/caramos-app-center-release.asc").read_bytes()


@unittest.skipUnless(shutil.which("gpg"), "needs gpg")
class KeyTests(unittest.TestCase):
    def test_accepts_anydesks_key(self) -> None:
        recipe.check_key(ANYDESK_KEY)
        self.assertEqual([recipe.FINGERPRINT], recipe.primary_fingerprints(ANYDESK_KEY))

    def test_rejects_another_key_or_one_more(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "not the expected one"):
            recipe.check_key(OTHER_KEY)
        with self.assertRaisesRegex(RuntimeError, "not the expected one"):
            recipe.check_key(ANYDESK_KEY + b"\n" + OTHER_KEY)

    def test_rejects_what_is_not_an_armored_key(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "not an armored public key"):
            recipe.check_key(b"<html>Moved</html>")


class SourceTests(unittest.TestCase):
    def test_the_repository_trusts_only_anydesks_key(self) -> None:
        self.assertEqual("deb [arch=amd64 signed-by=/etc/apt/keyrings/keys.anydesk.com.asc] https://deb.anydesk.com all main\n",
                         recipe.source_line())
        self.assertEqual(Path("/etc/apt/sources.list.d/anydesk-stable.list"), recipe.SOURCE)


class Run:
    """Fake subprocess.run: records the commands, fails the ones whose last word is in fail."""

    def __init__(self, fail: tuple[str, ...] = (), installed: bool = False) -> None:
        self.calls: list[list[str]] = []
        self.fail = fail
        self.installed = installed

    def __call__(self, args, **_kwargs):
        self.calls.append(list(args))
        if args[0] == "dpkg-query":
            return type("Done", (), {"returncode": 0, "stdout": "installed" if self.installed else ""})()
        code = 100 if args[-1] in self.fail else 0
        return type("Done", (), {"returncode": code, "stdout": ""})()


def fake_check(key: bytes) -> None:
    """check_key without gpg (KeyTests checks the real one)."""

    if key != ANYDESK_KEY:
        raise RuntimeError("the signing key AnyDesk sent is not the expected one")


class InstallTests(unittest.TestCase):
    def setUp(self) -> None:
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        self.keyring = self.root / "keyrings/keys.anydesk.com.asc"
        self.source = self.root / "sources.list.d/anydesk-stable.list"
        self.settings = self.root / "anydesk"
        patcher = patch.multiple(recipe, KEYRING=self.keyring, SOURCE=self.source, SETTINGS=self.settings)
        patcher.start()
        self.addCleanup(patcher.stop)

    def run_main(self, action: str, run: Run, key: bytes = ANYDESK_KEY) -> tuple[int, list[str]]:
        out = io.StringIO()
        with patch.object(recipe, "fetch_key", return_value=key), patch.object(recipe, "check_key", side_effect=fake_check), \
                patch.object(recipe.subprocess, "run", side_effect=run), contextlib.redirect_stdout(out):
            code = recipe.main(["anydesk", action])
        return code, out.getvalue().splitlines()

    def test_install_reads_only_anydesks_lists_then_installs(self) -> None:
        run = Run()
        code, lines = self.run_main("install", run)
        self.assertEqual(0, code, lines)
        self.assertEqual(ANYDESK_KEY, self.keyring.read_bytes())
        self.assertIn(f"signed-by={self.keyring}", self.source.read_text(encoding="utf-8"))
        update = run.calls[0]
        self.assertEqual("update", update[-1])
        self.assertIn(f"Dir::Etc::sourcelist={self.source}", update)
        self.assertIn("Dir::Etc::sourceparts=-", update)
        self.assertEqual(["install", "-y", "anydesk"], run.calls[1][-3:])
        self.assertEqual(0o644, self.keyring.stat().st_mode & 0o777)

    def test_a_wrong_key_changes_nothing(self) -> None:
        run = Run()
        code, lines = self.run_main("install", run, key=b"-----BEGIN PGP PUBLIC KEY BLOCK-----\nother")
        self.assertEqual(1, code)
        self.assertEqual("AnyDesk could not be installed: the signing key AnyDesk sent is not the expected one", lines[-1])
        self.assertFalse(self.keyring.exists() or self.source.exists())
        self.assertEqual([], run.calls)

    def test_a_failed_install_takes_the_repository_away(self) -> None:
        run = Run(fail=("anydesk",))
        code, lines = self.run_main("install", run)
        self.assertEqual(1, code)
        self.assertEqual("AnyDesk could not be installed: apt-get could not install anydesk", lines[-1])
        self.assertFalse(self.keyring.exists() or self.source.exists())

    def test_an_unreachable_repository_is_taken_away(self) -> None:
        code, lines = self.run_main("install", Run(fail=("update",)))
        self.assertEqual(1, code)
        self.assertEqual("AnyDesk could not be installed: apt-get could not read AnyDesk's repository", lines[-1])
        self.assertFalse(self.keyring.exists() or self.source.exists())

    def test_remove_takes_the_package_its_settings_the_repository_and_the_key(self) -> None:
        self.run_main("install", Run())
        # What the service keeps: this computer's AnyDesk address, which must not outlive the app.
        self.settings.mkdir()
        (self.settings / "system.conf").write_text("ad.anynet.id=123456789\n", encoding="utf-8")
        run = Run()
        code, _lines = self.run_main("remove", run)
        self.assertEqual(0, code)
        self.assertEqual(["purge", "-y", "anydesk"], run.calls[0][-3:])
        self.assertFalse(self.keyring.exists() or self.source.exists() or self.settings.exists())

    def test_remove_does_not_follow_a_link_in_place_of_the_settings(self) -> None:
        target = self.root / "elsewhere"
        target.mkdir()
        (target / "keep").write_text("x", encoding="utf-8")
        self.settings.symlink_to(target)
        code, _lines = self.run_main("remove", Run())
        self.assertEqual(0, code)
        self.assertFalse(self.settings.is_symlink())
        self.assertTrue((target / "keep").exists())


if __name__ == "__main__":
    unittest.main()
