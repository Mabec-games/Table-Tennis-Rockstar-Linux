import hashlib
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'project/tools'))
from disc_image import DiscImage, MAGIC, extract
from patch_native import apply_edits
from patch_menu import patch
from package_source import audit


def synthetic_iso(directory_sector=40, directory_size=64):
    image = bytearray(44 * 2048)
    descriptor = 32 * 2048
    image[descriptor:descriptor + 20] = MAGIC
    image[descriptor + 2048 - 20:descriptor + 2048] = MAGIC
    struct.pack_into('<II', image, descriptor + 20, directory_sector, directory_size)
    name = b'sample.txt'
    struct.pack_into('<HHIIBB', image, 40 * 2048, 0, 0, 42, 3, 0, len(name))
    image[40 * 2048 + 14:40 * 2048 + 14 + len(name)] = name
    image[42 * 2048:42 * 2048 + 3] = b'abc'
    return image


class LocalBuildTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def iso(self, data):
        path = self.root / 'synthetic.iso'
        path.write_bytes(data)
        return path

    def test_reads_case_insensitive_without_modifying_iso(self):
        path = self.iso(synthetic_iso())
        before = path.read_bytes()
        with DiscImage(path) as disc:
            self.assertEqual(disc.file('SAMPLE.TXT', 3), b'abc')
        self.assertEqual(path.read_bytes(), before)

    def test_out_of_bounds_directory_rejected(self):
        with DiscImage(self.iso(synthetic_iso(directory_sector=1000))) as disc:
            with self.assertRaisesRegex(ValueError, 'outside'):
                disc.file('sample.txt', 3)

    def test_cyclic_directory_rejected(self):
        data = synthetic_iso()
        base = 40 * 2048
        struct.pack_into('<HH', data, base, 8, 0)
        struct.pack_into('<HHIIBB', data, base + 32, 8, 0, 42, 3, 0, 1)
        data[base + 46] = ord('b')
        with DiscImage(self.iso(data)) as disc:
            with self.assertRaisesRegex(ValueError, 'cyclic'):
                disc.file('a', 3)

    def test_wrong_game_creates_no_output(self):
        data = synthetic_iso()
        name = b'default.xex'
        data[40 * 2048 + 13] = len(name)
        data[40 * 2048 + 14:40 * 2048 + 14 + len(name)] = name
        with self.assertRaisesRegex(ValueError, 'Unsupported game executable'):
            extract(self.iso(data), self.root / 'output')
        self.assertFalse((self.root / 'output').exists())

    def test_native_edit_checks_input_and_is_idempotent(self):
        digest = lambda text: hashlib.sha256(text.encode()).hexdigest()
        rule = {'name': 'synthetic', 'input_sha256': digest('abc'), 'output_sha256': digest('axc'),
                'edits': [{'offset': 1, 'delete': 1, 'insert': 'x'}]}
        self.assertEqual(apply_edits('abc', rule), 'axc')
        self.assertEqual(apply_edits('axc', rule), 'axc')
        with self.assertRaisesRegex(ValueError, 'differs'):
            apply_edits('abd', rule)

    def test_unsupported_menu_unchanged(self):
        path = self.root / 'menu.rpf'
        path.write_bytes(b'unsupported input')
        with self.assertRaises(ValueError):
            patch(path)
        self.assertEqual(path.read_bytes(), b'unsupported input')

    def test_public_allowlist_excludes_all_local_game_output(self):
        names = audit()
        self.assertFalse(any(n.startswith(('dist/', 'private_work/', 'project/generated/', 'sdk/', '.tools/')) for n in names))
        self.assertFalse(any(Path(n).suffix in ('.exe', '.dll', '.xex', '.rpf', '.bin', '.iso') for n in names))


if __name__ == '__main__':
    unittest.main()
