"""Translation checks: English source strings in the UI, every one translated in vi.po."""

from __future__ import annotations

import ast
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from caramos_app_center.i18n import prefers_vietnamese, ui_language

ROOT = Path(__file__).parents[1]
DIST = ROOT / "usr/lib/python3/dist-packages"
PO = ROOT / "po/caramos-app-center/vi.po"
POT = ROOT / "po/caramos-app-center/caramos-app-center.pot"
UI_SOURCES = sorted((DIST / "caramos_app_center/ui").glob("*.py"))
VIETNAMESE = re.compile(r"[À-ỹĐđ]")


def msgids(path: Path) -> set[str]:
    ids: set[str] = set()
    current: list[str] | None = None
    for line in path.read_text(encoding="utf-8").splitlines() + [""]:
        if line.startswith("msgid "):
            current = [line[6:]]
        elif current is not None and line.startswith('"'):
            current.append(line)
        else:
            if current is not None:
                text = "".join(ast.literal_eval(part) for part in current)
                if text:
                    ids.add(text)
            current = None
    return ids


def source_msgids(path: Path) -> set[str]:
    ids: set[str] = set()
    for node in ast.walk(ast.parse(path.read_text(encoding="utf-8"))):
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id in {"_", "ngettext"}:
            first = node.args[0] if node.args else None
            if isinstance(first, ast.Constant) and isinstance(first.value, str):
                ids.add(first.value)
    return ids


class LanguageTests(unittest.TestCase):
    def test_language_follows_gettext_variable_order(self) -> None:
        self.assertEqual("vi", ui_language({"LANGUAGE": "vi_VN:vi", "LANG": "en_US.UTF-8"}))
        self.assertEqual("en", ui_language({"LANGUAGE": "en_US", "LC_ALL": "vi_VN.UTF-8"}))
        self.assertEqual("vi", ui_language({"LANG": "vi_VN"}))
        self.assertEqual("en", ui_language({"LC_ALL": "C.UTF-8", "LANG": "vi_VN.UTF-8"}))
        self.assertFalse(prefers_vietnamese({}))


class CatalogFileTests(unittest.TestCase):
    def test_ui_has_no_vietnamese_literals(self) -> None:
        for path in UI_SOURCES:
            for node in ast.walk(ast.parse(path.read_text(encoding="utf-8"))):
                if isinstance(node, ast.Constant) and isinstance(node.value, str) and VIETNAMESE.search(node.value):
                    self.fail(f"{path.name}:{node.lineno}: Vietnamese literal; use _() with English")

    def test_every_ui_msgid_is_translated_and_the_template_is_current(self) -> None:
        source = set().union(*(source_msgids(path) for path in UI_SOURCES))
        self.assertEqual([], sorted(source - msgids(PO)), "run: make pot, then translate")
        self.assertEqual(msgids(POT), msgids(PO))
        self.assertEqual(source, msgids(POT), "the .pot is stale; run: make pot")

    def test_po_compiles_and_vietnamese_sessions_get_it(self) -> None:
        if shutil.which("msgfmt") is None:
            self.skipTest("msgfmt not installed")
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp) / "vi/LC_MESSAGES"
            target.mkdir(parents=True)
            subprocess.run(["msgfmt", "--check", "-o", str(target / "caramos-app-center.mo"), str(PO)], check=True)
            code = "from caramos_app_center.i18n import _\nprint(_('Install'))\n"
            for language, expected in (("vi_VN:vi", "Cài đặt"), ("en_US:en", "Install")):
                env = dict(os.environ, CARAMOS_APP_CENTER_LOCALEDIR=tmp, LANGUAGE=language, PYTHONPATH=str(DIST))
                output = subprocess.run([sys.executable, "-c", code], env=env, check=True, capture_output=True, text=True)
                self.assertEqual(expected, output.stdout.strip())


if __name__ == "__main__":
    unittest.main()
