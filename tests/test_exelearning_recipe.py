"""The eXeLearning recipe: which release it installs, how it checks the download, and how it reports a failure."""

from __future__ import annotations

import contextlib
import hashlib
import importlib.machinery
import importlib.util
import io
import tempfile
import unittest
import urllib.error
from pathlib import Path
from unittest.mock import patch

RECIPE = Path(__file__).parents[1] / "usr/share/caramos-app-center/recipes/exelearning"
_loader = importlib.machinery.SourceFileLoader("exelearning_recipe", str(RECIPE))
recipe = importlib.util.module_from_spec(importlib.util.spec_from_loader(_loader.name, _loader))
_loader.exec_module(recipe)

DOWNLOAD = "https://github.com/exelearning/exelearning/releases/download"


def asset(tag: str, name: str, digest: str = "sha256:" + "0" * 64, size: int = 125582972) -> dict:
    return {"name": name, "size": size, "digest": digest, "browser_download_url": f"{DOWNLOAD}/{tag}/{name}"}


def release(tag: str, *assets: dict, prerelease: bool = False, draft: bool = False) -> dict:
    return {"tag_name": tag, "draft": draft, "prerelease": prerelease, "assets": list(assets)}


# What GitHub listed on 2026-11-25 (a part of it): the -rc and -beta tags are not marked as pre-releases.
R_406 = release("v4.0.6",
                asset("v4.0.6", "eXeLearning-4.0.6.msi", size=152010752),
                asset("v4.0.6", "exelearning-4.0.6.x86_64.rpm", size=113507829),
                asset("v4.0.6", "exelearning_4.0.6_amd64.deb",
                      "sha256:0e73033d084f681a2c271d02b74039f7d23d2d1133f22a9d1997d73cfc6d294d", 125582972),
                asset("v4.0.6", "latest-linux.yml", size=510))
R_405 = release("v4.0.5", asset("v4.0.5", "exelearning_4.0.5_amd64.deb",
                                "sha256:d1a263f5c5b7d55a8de6bd4f2022c3deb95f930fc88a0eee6ba51de048787beb", 121400320))
R_RC3 = release("v4.0.0-rc3", asset("v4.0.0-rc3", "exelearning_4.0.0-rc3_amd64.deb", size=120066012))


class NewestAssetTests(unittest.TestCase):
    def test_takes_the_deb_of_the_newest_stable_release(self) -> None:
        found = recipe.newest_asset([R_RC3, R_405, R_406])
        self.assertEqual(recipe.Asset("exelearning_4.0.6_amd64.deb", f"{DOWNLOAD}/v4.0.6/exelearning_4.0.6_amd64.deb",
                                      "0e73033d084f681a2c271d02b74039f7d23d2d1133f22a9d1997d73cfc6d294d", 125582972,
                                      "4.0.6"), found)

    def test_never_a_beta_rc_draft_or_pre_release(self) -> None:
        beta = release("v4.1.0-beta1", asset("v4.1.0-beta1", "exelearning_4.1.0-beta1_amd64.deb"))
        draft = release("v4.1.0", asset("v4.1.0", "exelearning_4.1.0_amd64.deb"), draft=True)
        pre = release("v4.1.1", asset("v4.1.1", "exelearning_4.1.1_amd64.deb"), prerelease=True)
        self.assertEqual("4.0.5", recipe.newest_asset([beta, draft, pre, R_RC3, R_405]).version)

    def test_needs_a_sha256_a_size_and_a_github_download_url(self) -> None:
        no_digest = release("v4.0.7", asset("v4.0.7", "exelearning_4.0.7_amd64.deb", digest=""))
        elsewhere = release("v4.0.8", dict(asset("v4.0.8", "exelearning_4.0.8_amd64.deb"),
                                           browser_download_url="https://example.com/exelearning_4.0.8_amd64.deb"))
        other_tag = release("v4.0.9", dict(asset("v4.0.9", "exelearning_4.0.9_amd64.deb"),
                                           browser_download_url=f"{DOWNLOAD}/v9.9.9/exelearning_4.0.9_amd64.deb"))
        no_size = release("v4.0.10", asset("v4.0.10", "exelearning_4.0.10_amd64.deb", size=0))
        self.assertEqual("4.0.6", recipe.newest_asset([no_digest, elsewhere, other_tag, no_size, R_406]).version)
        with self.assertRaisesRegex(RuntimeError, "amd64 .deb"):
            recipe.newest_asset([no_digest, elsewhere, other_tag, no_size])
        with self.assertRaises(RuntimeError):
            recipe.newest_asset({"message": "API rate limit exceeded"})

    def test_versions_compare_as_numbers(self) -> None:
        r_4010 = release("v4.0.10", asset("v4.0.10", "exelearning_4.0.10_amd64.deb"))
        self.assertEqual("4.0.10", recipe.newest_asset([R_406, r_4010]).version)

    def test_the_pinned_release_when_github_does_not_answer(self) -> None:
        limited = urllib.error.HTTPError(recipe.API_URL, 403, "rate limit exceeded", {}, None)
        with patch.object(recipe, "fetch_releases", side_effect=limited), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(recipe.PINNED, recipe.choose_asset())
        self.assertTrue(recipe.PINNED.url.startswith(f"{DOWNLOAD}/v{recipe.PINNED.version}/"))


class FakeResponse(io.BytesIO):
    def __enter__(self):
        return self

    def __exit__(self, *_exc) -> None:
        self.close()


class DownloadTests(unittest.TestCase):
    def download(self, body: bytes, sha256: str, size: int) -> Path:
        fake = recipe.Asset("exelearning_9.9.9_amd64.deb", f"{DOWNLOAD}/v9.9.9/exelearning_9.9.9_amd64.deb", sha256,
                            size, "9.9.9")
        with patch.object(recipe.urllib.request, "urlopen", return_value=FakeResponse(body)), \
                contextlib.redirect_stdout(io.StringIO()):
            return recipe.download(fake, Path(self.tmp.name))

    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)

    def test_a_file_that_matches_is_kept(self) -> None:
        body = b"debian package"
        path = self.download(body, hashlib.sha256(body).hexdigest(), len(body))
        self.assertEqual(body, path.read_bytes())

    def test_a_file_that_does_not_match_is_deleted(self) -> None:
        body = b"something else"
        with self.assertRaisesRegex(RuntimeError, "does not match"):
            self.download(body, "0" * 64, len(body))
        with self.assertRaisesRegex(RuntimeError, "does not match"):
            self.download(body + b"more", hashlib.sha256(body).hexdigest(), len(body))
        self.assertEqual([], list(Path(self.tmp.name).iterdir()))


class MainTests(unittest.TestCase):
    def run_main(self, action: str) -> tuple[int, list[str]]:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = recipe.main(["exelearning", action])
        return code, out.getvalue().splitlines()

    def test_only_the_exelearning_package_is_installed(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            staged = Path(tmp) / "x.deb"
            staged.write_bytes(b"deb")
            calls = []
            with patch.object(recipe, "choose_asset", return_value=recipe.PINNED), \
                    patch.object(recipe, "download", return_value=staged), \
                    patch.object(recipe, "package_name", return_value="something-else"), \
                    patch.object(recipe.subprocess, "run", side_effect=lambda args, **_kw: calls.append(args)):
                code, lines = self.run_main("install")
        self.assertEqual(1, code)
        self.assertEqual("eXeLearning could not be installed: exelearning_4.0.6_amd64.deb is not the exelearning package",
                         lines[-1])
        self.assertEqual([], calls)

    def test_install_hands_the_checked_deb_to_apt(self) -> None:
        calls = []

        def fake_run(args, **_kwargs):
            calls.append(args)
            return type("Done", (), {"returncode": 0})()

        with tempfile.TemporaryDirectory() as tmp:
            staged = Path(tmp) / "exelearning_4.0.6_amd64.deb"
            staged.write_bytes(b"deb")
            with patch.object(recipe, "choose_asset", return_value=recipe.PINNED), \
                    patch.object(recipe, "download", return_value=staged), \
                    patch.object(recipe, "package_name", return_value="exelearning"), \
                    patch.object(recipe.subprocess, "run", side_effect=fake_run):
                code, lines = self.run_main("install")
        self.assertEqual(0, code)
        self.assertEqual(["install", "-y", str(staged)], calls[0][-3:])
        self.assertIn("PROGRESS 100", lines)

    def test_a_failure_ends_with_one_line_saying_why(self) -> None:
        offline = urllib.error.URLError("[Errno -3] Temporary failure in name resolution")
        with patch.object(recipe, "fetch_releases", side_effect=offline), \
                patch.object(recipe.urllib.request, "urlopen", side_effect=offline):
            code, lines = self.run_main("install")
        self.assertEqual(1, code)
        self.assertEqual("eXeLearning could not be installed: <urlopen error [Errno -3] Temporary failure in name "
                         "resolution>", lines[-1])

    def test_remove_removes_the_package(self) -> None:
        calls = []

        def fake_run(args, **_kwargs):
            calls.append(args)
            return type("Done", (), {"returncode": 0})()

        with patch.object(recipe.subprocess, "run", side_effect=fake_run):
            code, _lines = self.run_main("remove")
        self.assertEqual(0, code)
        self.assertEqual(["remove", "-y", "exelearning"], calls[0][-3:])


if __name__ == "__main__":
    unittest.main()
