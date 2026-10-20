import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    'appimage_entry', Path(__file__).resolve().parents[1] / 'project/launcher/appimage_entry.py')
entry = importlib.util.module_from_spec(spec)
spec.loader.exec_module(entry)


class AppImageStagingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.bundle = self.root / 'read only image'
        self.game = self.bundle / 'build/linux/tabletennis'
        self.game.parent.mkdir(parents=True)
        self.game.write_bytes(b'test executable')
        self.game.chmod(0o755)
        self.manifest = self.bundle / 'runtime-manifest.json'
        self.manifest.write_text(json.dumps({
            'tabletennis': hashlib.sha256(self.game.read_bytes()).hexdigest()}))

    def test_versioned_staging_preserves_writable_settings(self):
        staged = entry.stage_runtime(self.bundle, self.root / 'cache')
        self.assertEqual(staged.read_bytes(), self.game.read_bytes())
        self.assertTrue(staged.stat().st_mode & 0o111)
        config = staged.parent / 'tabletennis.toml'
        config.write_text('test settings')
        self.assertEqual(entry.stage_runtime(self.bundle, self.root / 'cache'), staged)
        self.assertEqual(config.read_text(), 'test settings')
        self.assertFalse((self.game.parent / 'tabletennis.toml').exists())

    def test_corrupt_payload_does_not_leave_a_runnable_install(self):
        self.game.write_bytes(b'corrupt')
        with self.assertRaisesRegex(ValueError, 'checksum mismatch'):
            entry.stage_runtime(self.bundle, self.root / 'cache')
        self.assertEqual(list((self.root / 'cache').iterdir()), [])


if __name__ == '__main__':
    unittest.main()
