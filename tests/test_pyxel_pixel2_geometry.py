import importlib.util
import os
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SHIM_PATH = ROOT / "package/pyxel-pixel2/src/plumos_pyxel_pixel2_shim.py"


spec = importlib.util.spec_from_file_location("plumos_pyxel_pixel2_shim", SHIM_PATH)
assert spec and spec.loader
shim = importlib.util.module_from_spec(spec)
spec.loader.exec_module(shim)


def fake_init(
    width,
    height,
    title=None,
    fps=30,
    quit_key=None,
    display_scale=None,
):
    return None


class Pixel2PyxelGeometryTests(unittest.TestCase):
    def setUp(self):
        self.saved = {
            name: os.environ.get(name)
            for name in ("PLUMOS_PYXEL_CANVAS_SIZE", "PLUMOS_PYXEL_LOGICAL_SIZE")
        }
        os.environ["PLUMOS_PYXEL_INIT_SHIM_LOG"] = "0"

    def tearDown(self):
        for name, value in self.saved.items():
            if value is None:
                os.environ.pop(name, None)
            else:
                os.environ[name] = value
        os.environ.pop("PLUMOS_PYXEL_INIT_SHIM_LOG", None)

    def publish(self, *args, **kwargs):
        shim._publish_pyxel_init_geometry(fake_init, args, kwargs)

    def test_last_emulator_keeps_full_720x480_source(self):
        self.publish(720, 480)
        self.assertEqual(os.environ["PLUMOS_PYXEL_CANVAS_SIZE"], "720x480")
        self.assertEqual(os.environ["PLUMOS_PYXEL_LOGICAL_SIZE"], "720x480")

    def test_keyword_geometry_is_supported(self):
        self.publish(width=320, height=180)
        self.assertEqual(os.environ["PLUMOS_PYXEL_LOGICAL_SIZE"], "320x180")

    def test_explicit_display_scale_is_included(self):
        self.publish(160, 120, display_scale=3)
        self.assertEqual(os.environ["PLUMOS_PYXEL_LOGICAL_SIZE"], "480x360")

    def test_invalid_geometry_keeps_fallback(self):
        os.environ["PLUMOS_PYXEL_LOGICAL_SIZE"] = "640x480"
        self.publish(0, 480)
        self.assertEqual(os.environ["PLUMOS_PYXEL_LOGICAL_SIZE"], "640x480")

    def test_stale_play_cleanup_preserves_live_process_directory(self):
        with tempfile.TemporaryDirectory() as tmp:
            play_dir = Path(tmp) / "play"
            dead = play_dir / "111_dead"
            live = play_dir / "222_live"
            dead.mkdir(parents=True)
            live.mkdir()
            (dead / "payload").write_text("dead", encoding="utf-8")
            (live / "payload").write_text("live", encoding="utf-8")

            shim._cleanup_stale_play_dirs(play_dir, lambda pid: pid == 222)

            self.assertFalse(dead.exists())
            self.assertTrue(live.exists())


if __name__ == "__main__":
    unittest.main()
