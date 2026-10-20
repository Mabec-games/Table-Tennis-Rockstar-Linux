"""Integration test against the installed Linux SDK: python3 tests/test_linux_memory_query.py."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SDK = Path(os.environ.get('TABLETENNIS_TEST_SDK', ROOT / '.tools/sdk-linux/linux-amd64'))


@unittest.skipUnless(sys.platform == 'linux' and shutil.which('clang++') and
                     (SDK / 'lib/librexruntime.so').is_file(), 'requires Clang and Linux ReXGlue SDK')
class MemoryQueryTests(unittest.TestCase):
    def test_kernel_query_and_legacy_fallback(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / 'memory-query-test'
            subprocess.run([
                'clang++', '-std=c++23', '-O2', '-pthread', '-I' + str(SDK / 'include'),
                str(ROOT / 'tests/linux_memory_query_test.cpp'),
                str(ROOT / 'project/src/linux_memory_query.cpp'),
                '-L' + str(SDK / 'lib'), '-Wl,-rpath,' + str(SDK / 'lib'),
                '-Wl,--export-dynamic-symbol=_ZN3rex6memory12QueryProtectEPvRmRNS0_10PageAccessE',
                '-lrexruntime', '-ldl', '-o', str(binary),
            ], check=True, timeout=60)
            for legacy in (False, True):
                with self.subTest(legacy=legacy):
                    env = os.environ.copy()
                    env.pop('TABLETENNIS_LEGACY_MEMORY_QUERY', None)
                    if legacy:
                        env['TABLETENNIS_LEGACY_MEMORY_QUERY'] = '1'
                    result = subprocess.run([str(binary)], env=env, text=True,
                                            capture_output=True, timeout=30)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertIn('PASS: permissions', result.stdout)
                    print(('Legacy fallback: ' if legacy else 'Kernel path: ') + result.stdout, end='')


if __name__ == '__main__':
    unittest.main()
