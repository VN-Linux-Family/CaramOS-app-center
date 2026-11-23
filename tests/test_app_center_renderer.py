"""caramos_app_center.renderer: cairo where OpenGL would run on the CPU or memory is short (as caramos-ota's
caramos_ota/gtk_renderer.py does for its own programs)."""

from __future__ import annotations

import unittest
from pathlib import Path
from unittest import mock

from caramos_app_center import renderer

ROOT = Path(__file__).parents[1]
GIB = 1024 * 1024
CARAMOS_OTA_COPY = ROOT.parent / "caramos-ota/usr/lib/python3/dist-packages/caramos_ota/gtk_renderer.py"


def code(path: Path) -> str:
    """The module without its docstring."""

    text = path.read_text(encoding="utf-8")
    return text[text.index('"""', 3) + 3:]


class RendererTests(unittest.TestCase):
    def test_choice(self) -> None:
        self.assertEqual("cairo", renderer.renderer({}, set(), 8 * GIB))
        self.assertEqual("cairo", renderer.renderer({}, {"i915"}, 2 * GIB))
        self.assertIsNone(renderer.renderer({}, {"i915"}, 8 * GIB))
        self.assertIsNone(renderer.renderer({"GSK_RENDERER": "ngl"}, set(), 2 * GIB))

    def test_apply(self) -> None:
        environ: dict[str, str] = {}
        with mock.patch.object(renderer, "gpu_drivers", return_value={"virtio_gpu"}), \
                mock.patch.object(renderer, "memory_kib", return_value=8 * GIB):
            self.assertEqual("cairo", renderer.apply(environ))
        self.assertEqual({"GSK_RENDERER": "cairo"}, environ)

    def test_before_gtk(self) -> None:
        source = (ROOT / "usr/lib/python3/dist-packages/caramos_app_center/ui/app.py").read_text(encoding="utf-8")
        main = source[source.index("def main("):]
        self.assertLess(main.index("renderer.apply()"), main.index('gi.require_version("Gtk", "4.0")'))

    @unittest.skipUnless(CARAMOS_OTA_COPY.is_file(), "caramos-ota is not next to this package")
    def test_same_rule_as_caramos_ota(self) -> None:
        self.assertEqual(code(CARAMOS_OTA_COPY), code(ROOT / "usr/lib/python3/dist-packages/caramos_app_center/renderer.py"))


if __name__ == "__main__":
    unittest.main()
