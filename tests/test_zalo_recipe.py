"""The Zalo recipe: which build of VN-Linux-Family/zalo-for-linux it installs, and how it reports a failure."""

from __future__ import annotations

import contextlib
import importlib.machinery
import importlib.util
import io
import json
import tempfile
import unittest
import urllib.error
from pathlib import Path
from unittest.mock import patch

RECIPE = Path(__file__).parents[1] / "usr/share/caramos-app-center/recipes/zalo-full"
_loader = importlib.machinery.SourceFileLoader("zalo_full_recipe", str(RECIPE))
recipe = importlib.util.module_from_spec(importlib.util.spec_from_loader(_loader.name, _loader))
_loader.exec_module(recipe)

DOWNLOAD = "https://github.com/VN-Linux-Family/zalo-for-linux/releases/download"


def asset(tag: str, name: str, digest: str = "sha256:" + "4" * 64, size: int = 167109987) -> dict:
    return {"name": name, "size": size, "digest": digest, "browser_download_url": f"{DOWNLOAD}/{tag}/{name}"}


def release(tag: str, *assets: dict, prerelease: bool = False) -> dict:
    return {"tag_name": tag, "draft": False, "prerelease": prerelease, "assets": list(assets)}


# What GitHub listed on 2026-10-18: 26.10.11 has no "-Full-" build any more; every build makes calls natively.
R_26_10_11 = release(
    "26.10.11",
    asset("26.10.11", "Zalo-26.10.11+ZaDark-26.2.1-285ba91-aarch64.AppImage"),
    asset("26.10.11", "Zalo-26.10.11+ZaDark-26.2.1-285ba91-x86_64.AppImage.zsync", size=285828),
    asset("26.10.11", "Zalo-26.10.11+ZaDark-26.2.1-285ba91-x86_64.AppImage",
          "sha256:44316b4441adc4d68375b48092eac0465a35885ec55611f44c7680b93315473b"),
    asset("26.10.11", "Zalo-26.10.11-Original-285ba91-x86_64.AppImage"),
)
R_26_10_10 = release(
    "26.10.10",
    asset("26.10.10", "Zalo-26.10.10+ZaDark-26.2.1-e80931a-Full-x86_64.AppImage", size=280539122),
    asset("26.10.10", "Zalo-26.10.10+ZaDark-26.2.1-e80931a-x86_64.AppImage", size=180385905),
)


class NewestAssetTests(unittest.TestCase):
    def test_takes_the_zadark_x86_64_build_of_the_latest_release(self) -> None:
        found = recipe.newest_asset([R_26_10_11, R_26_10_10])
        self.assertEqual("Zalo-26.10.11+ZaDark-26.2.1-285ba91-x86_64.AppImage", found.name)
        self.assertEqual("26.10.11", found.tag)
        self.assertEqual("44316b4441adc4d68375b48092eac0465a35885ec55611f44c7680b93315473b", found.sha256)
        self.assertEqual(f"{DOWNLOAD}/26.10.11/{found.name}", found.url)
        self.assertEqual(167109987, found.size)

    def test_never_the_old_wine_full_build(self) -> None:
        found = recipe.newest_asset([R_26_10_10])
        self.assertEqual("Zalo-26.10.10+ZaDark-26.2.1-e80931a-x86_64.AppImage", found.name)

    def test_a_release_without_the_build_falls_back_to_an_older_one(self) -> None:
        uploading = release("26.10.12", asset("26.10.12", "Zalo-26.10.12-Original-1234567-x86_64.AppImage"))
        beta = release("26.10.13", asset("26.10.13", "Zalo-26.10.13+ZaDark-26.2.1-7654321-x86_64.AppImage"),
                       prerelease=True)
        self.assertEqual("26.10.11", recipe.newest_asset([beta, uploading, R_26_10_11]).tag)

    def test_needs_a_sha256_and_a_github_download_url(self) -> None:
        no_digest = release("26.10.12", asset("26.10.12", "Zalo-26.10.12+ZaDark-26.2.1-1234567-x86_64.AppImage",
                                              digest=""))
        elsewhere = release("26.10.12", dict(asset("26.10.12", "Zalo-26.10.12+ZaDark-26.2.1-1234567-x86_64.AppImage"),
                                             browser_download_url="https://example.com/Zalo.AppImage"))
        self.assertEqual("26.10.11", recipe.newest_asset([no_digest, elsewhere, R_26_10_11]).tag)
        with self.assertRaisesRegex(RuntimeError, "ZaDark x86_64"):
            recipe.newest_asset([no_digest, elsewhere])
        with self.assertRaises(RuntimeError):
            recipe.newest_asset({"message": "API rate limit exceeded"})


class MainTests(unittest.TestCase):
    def run_main(self, action: str) -> tuple[int, list[str]]:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = recipe.main(["zalo-full", action])
        return code, out.getvalue().splitlines()

    def test_a_failure_ends_with_one_line_saying_why(self) -> None:
        offline = urllib.error.URLError("[Errno -3] Temporary failure in name resolution")
        with patch.object(recipe, "fetch_releases", side_effect=offline):
            code, lines = self.run_main("install")
        self.assertEqual(1, code)
        self.assertEqual("Zalo could not be installed: <urlopen error [Errno -3] Temporary failure in name resolution>",
                         lines[-1])

    def test_install_records_the_build_with_calls(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            staged = root / "staged"
            staged.write_bytes(b"appimage")
            with patch.multiple(recipe, APPIMAGE=root / "Zalo.AppImage", LAUNCHER=root / "zalo.desktop",
                                STATE_FILE=root / "state/zalo-appimage.json"), \
                    patch.object(recipe, "fetch_releases", return_value=[R_26_10_11]), \
                    patch.object(recipe, "missing_packages", return_value=[]), \
                    patch.object(recipe, "download", return_value=staged):
                code, lines = self.run_main("install")
            self.assertEqual(0, code)
            state = json.loads((root / "state/zalo-appimage.json").read_text(encoding="utf-8"))
            self.assertEqual({"repository": "VN-Linux-Family/zalo-for-linux", "tag": "26.10.11",
                              "asset": "Zalo-26.10.11+ZaDark-26.2.1-285ba91-x86_64.AppImage",
                              "sha256": "44316b4441adc4d68375b48092eac0465a35885ec55611f44c7680b93315473b",
                              "flavour": "full"}, state)
            self.assertEqual(b"appimage", (root / "Zalo.AppImage").read_bytes())
            self.assertIn("Exec=", (root / "zalo.desktop").read_text(encoding="utf-8"))
            self.assertIn("PROGRESS 100", lines)

    def test_installs_the_call_packages_that_are_missing(self) -> None:
        calls = []

        def fake_run(args, **_kwargs):
            calls.append(args)
            return type("Done", (), {"returncode": 0})()

        with patch.object(recipe, "missing_packages", return_value=["libopus0"]), \
                patch.object(recipe.subprocess, "run", side_effect=fake_run), \
                contextlib.redirect_stdout(io.StringIO()):
            recipe.install_call_packages()
        self.assertEqual("update", calls[0][-1])
        self.assertEqual(["install", "-y", "--", "libopus0"], calls[1][-4:])


if __name__ == "__main__":
    unittest.main()
