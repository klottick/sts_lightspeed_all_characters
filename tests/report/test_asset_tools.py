import contextlib
import importlib.util
import io
import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools" / "prepare_report_assets.py"
SOURCES = ROOT / "report" / "assets" / "sources.json"
PNG = b"\x89PNG\r\n\x1a\n" + b"test image"


def load_tool():
    spec = importlib.util.spec_from_file_location("prepare_report_assets", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class AssetToolTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.output = Path(self.temp.name) / "cache"

    def run_tool(self, *args):
        tool = load_tool()
        stdout, stderr = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            status = tool.main(["--sources", str(SOURCES), "--output", str(self.output), *args])
        return status, stdout.getvalue(), stderr.getvalue()

    def response(self, content):
        response = mock.MagicMock()
        response.read.return_value = content
        response.__enter__.return_value = response
        return response

    def test_catalog_has_exact_stable_keys_and_urls(self):
        catalog = json.loads(SOURCES.read_text())
        self.assertEqual(set(catalog), {
            "character/IRONCLAD", "monster/RED_LOUSE", "monster/GREEN_LOUSE",
            "card/BASH", "card/STRIKE_RED", "card/DEFEND_RED", "card/CLEAVE",
            "card/SHRUG_IT_OFF", "card/ANGER", "card/FLEX",
            "relic/BURNING_BLOOD", "relic/ODDLY_SMOOTH_STONE",
            "potion/FIRE_POTION", "potion/BLOCK_POTION", "ui/energy/red",
            "status/BLOCK", "status/CURL_UP", "status/WEAK", "status/VULNERABLE",
            "status/DEXTERITY", "intent/attack", "intent/debuff", "intent/buff",
        })
        self.assertEqual(len(catalog), 23)
        self.assertTrue(all(url.startswith("https://slaythespire.wiki.gg/images/") and url.endswith(".png") for url in catalog.values()))

    def test_missing_required_arguments_returns_usage_error(self):
        tool = load_tool()
        stderr = io.StringIO()
        with contextlib.redirect_stderr(stderr):
            status = tool.main([])
        self.assertEqual(status, 2)
        self.assertIn("usage:", stderr.getvalue().lower())

    def test_mock_download_is_idempotent_and_writes_manifest(self):
        tool = load_tool()
        with mock.patch.object(tool.urllib.request, "urlopen", return_value=self.response(PNG)) as urlopen:
            status, _, err = self.run_tool()
            self.assertEqual(status, 0, err)
            first_manifest = (self.output / "manifest.json").read_bytes()
            status, _, err = self.run_tool()
        self.assertEqual(status, 0, err)
        self.assertEqual(urlopen.call_count, 23)
        self.assertEqual((self.output / "manifest.json").read_bytes(), first_manifest)
        manifest = json.loads(first_manifest)
        self.assertEqual(manifest["schemaVersion"], 1)
        self.assertEqual(manifest["attribution"], "Game artwork © Mega Crit; sourced from Slay the Spire Wiki")
        self.assertEqual(len(manifest["assets"]), 23)
        self.assertEqual(set(manifest["assets"]), set(json.loads(SOURCES.read_text())))
        for entry in manifest["assets"].values():
            self.assertEqual(Path(entry["path"]).name, entry["path"])
            self.assertTrue((self.output / entry["path"]).is_file())

    def test_corrupt_download_is_rejected_without_manifest_entry(self):
        with mock.patch("urllib.request.urlopen", return_value=self.response(b"<svg></svg>")):
            status, _, err = self.run_tool()
        self.assertEqual(status, 1)
        self.assertIn("unsupported image signature", err)
        self.assertFalse((self.output / "manifest.json").exists())

    def test_source_path_traversal_is_rejected(self):
        source = Path(self.temp.name) / "sources.json"
        source.write_text(json.dumps({"bad/key": "https://slaythespire.wiki.gg/images/../escape.png"}))
        tool = load_tool()
        stderr = io.StringIO()
        with mock.patch.object(tool.urllib.request, "urlopen") as urlopen:
            with contextlib.redirect_stderr(stderr):
                status = tool.main(["--sources", str(source), "--output", str(self.output)])
        err = stderr.getvalue()
        self.assertEqual(status, 1)
        self.assertIn("unsafe", err.lower())
        urlopen.assert_not_called()

    def test_local_import_is_partial_and_missing_assets_are_warnings(self):
        local = Path(self.temp.name) / "local"
        local.mkdir()
        (local / "Ironclad.png").write_bytes(PNG)
        status, out, err = self.run_tool("--local-dir", str(local))
        self.assertEqual(status, 0, err)
        manifest = json.loads((self.output / "manifest.json").read_text())
        self.assertEqual(set(manifest["assets"]), {"character/IRONCLAD"})
        self.assertIn("warning", (out + err).lower())
        self.assertTrue((self.output / manifest["assets"]["character/IRONCLAD"]["path"]).is_file())

    def test_local_import_rejects_corrupt_file(self):
        local = Path(self.temp.name) / "local"
        local.mkdir()
        (local / "Ironclad.png").write_bytes(b"not an image")
        status, _, err = self.run_tool("--local-dir", str(local))
        self.assertEqual(status, 1)
        self.assertIn("unsupported image signature", err)

    def test_unrelated_output_file_is_not_overwritten(self):
        self.output.mkdir()
        sentinel = self.output / "notes.txt"
        sentinel.write_text("preserve")
        local = Path(self.temp.name) / "local"
        local.mkdir()
        status, _, err = self.run_tool("--local-dir", str(local))
        self.assertEqual(status, 1)
        self.assertIn("not empty", err.lower())
        self.assertEqual(sentinel.read_text(), "preserve")

    def test_output_symlinks_cannot_overwrite_unrelated_files(self):
        local = Path(self.temp.name) / "local"; local.mkdir()
        (local / "Ironclad.png").write_bytes(PNG)
        sentinel = Path(self.temp.name) / "user-data"; sentinel.write_bytes(b"preserve")
        for name in ("Ironclad.png", "manifest.json"):
            with self.subTest(name=name):
                self.output = Path(self.temp.name) / ("cache-" + name); self.output.mkdir()
                link = self.output / name; link.symlink_to(sentinel)
                try:
                    status, _, err = self.run_tool("--local-dir", str(local))
                    self.assertNotEqual(status, 0)
                    self.assertIn("symlink", err.lower())
                    self.assertEqual(sentinel.read_bytes(), b"preserve")
                finally:
                    link.unlink()


if __name__ == "__main__":
    unittest.main()
