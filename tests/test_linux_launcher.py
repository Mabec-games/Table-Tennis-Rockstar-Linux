import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch

spec = importlib.util.spec_from_file_location(
    'launcher_linux', Path(__file__).resolve().parents[1] / 'project/launcher/launcher_linux.py')
launcher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(launcher)


class LinuxLauncherTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.iso = Path(self.temp.name) / 'disc with spaces;$(echo test).ISO'
        self.iso.touch()
        self.settings = dict(launcher.DEFAULTS, iso=str(self.iso))

    def test_iso_path_is_one_literal_argument(self):
        args = launcher.arguments(self.settings)
        self.assertEqual(args[0], f'--game_data_root={self.iso}')

    def test_higher_resolution_scales_internal_rendering(self):
        args = launcher.arguments(dict(self.settings, resolution='4K'))
        self.assertIn('--draw_resolution_scale_x=3', args)
        self.assertIn('--draw_resolution_scale_y=3', args)
        self.assertIn('--window_width=3840', args)

    def test_steam_deck_800p_preset_and_alias(self):
        for res in ('800p (Steam Deck)', '800p'):
            args = launcher.arguments(dict(self.settings, resolution=res))
            self.assertIn('--window_width=1280', args)
            self.assertIn('--window_height=800', args)
            self.assertIn('--resolution_scale=1', args)
            self.assertIn('--draw_resolution_scale_x=1', args)
            self.assertIn('--draw_resolution_scale_y=1', args)
        self.assertEqual(launcher.normalize_resolution('800p'), '800p (Steam Deck)')

    def test_unsupported_filters_are_rejected(self):
        with self.assertRaises(ValueError):
            launcher.arguments(dict(self.settings, aa='invalid'))

    def test_directory_is_not_a_disc_image(self):
        folder = Path(self.temp.name) / 'folder.iso'
        folder.mkdir()
        with self.assertRaises(ValueError):
            launcher.arguments(dict(self.settings, iso=str(folder)))

    def test_gpu_index_is_saved_as_a_launch_override(self):
        self.assertEqual(launcher.selected_device(dict(self.settings, gpu='1'), []), 1)
        self.assertIsNone(launcher.selected_device(self.settings, []))

    def test_vulkaninfo_summary_is_parsed(self):
        output = '''\
Devices:
========
GPU0:
\tdeviceType         = PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU
\tdeviceName         = AMD Radeon 680M (RADV REMBRANDT)
GPU1:
\tdeviceType         = PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
\tdeviceName         = NVIDIA GeForce RTX 3080 Laptop GPU
'''
        self.assertEqual(launcher.enumerate_vulkan_devices(output), [
            (0, 'AMD Radeon 680M (RADV REMBRANDT)', 'PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU'),
            (1, 'NVIDIA GeForce RTX 3080 Laptop GPU', 'PHYSICAL_DEVICE_TYPE_DISCRETE_GPU')])

    def test_auto_prefers_discrete_even_when_integrated_gpu_is_first(self):
        devices = [(0, 'AMD Radeon Integrated Graphics', 'PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU'),
                   (1, 'Intel Arc A770', 'PHYSICAL_DEVICE_TYPE_DISCRETE_GPU')]
        self.assertEqual(launcher.selected_device(self.settings, devices), 1)

    def test_saved_gpu_is_resolved_by_name_after_enumeration_reorders(self):
        devices = [(0, 'AMD Radeon 680M', 'PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU'),
                   (1, 'NVIDIA RTX', 'PHYSICAL_DEVICE_TYPE_DISCRETE_GPU')]
        settings = dict(self.settings, gpu='device:AMD Radeon 680M')
        self.assertEqual(launcher.selected_device(settings, devices), 0)

    def test_bundled_runtime_libraries_precede_machine_sdk_paths(self):
        game = Path(self.temp.name) / 'package' / 'tabletennis'
        environment = launcher.runtime_environment(
            game, {'LD_LIBRARY_PATH': '/opt/rexglue-sdk/lib', 'PATH': '/usr/bin'})
        self.assertEqual(environment['LD_LIBRARY_PATH'],
                         str(game.parent.resolve()) + ':/opt/rexglue-sdk/lib')
        self.assertEqual(environment['PATH'], '/usr/bin')

    def test_appimage_wait_keeps_parent_alive_and_returns_game_exit_code(self):
        child = Mock()
        child.wait.return_value = 7
        with patch.object(launcher.sys, 'argv', ['launcher', '--play', '--wait']), \
                patch.object(launcher, 'CONFIG', Path(self.temp.name) / 'settings.json'), \
                patch.object(launcher, 'launch', return_value=child):
            self.assertEqual(launcher.main(), 7)
        child.wait.assert_called_once_with()

    def test_normal_cli_launch_still_returns_without_waiting(self):
        child = Mock()
        with patch.object(launcher.sys, 'argv', ['launcher', '--play']), \
                patch.object(launcher, 'CONFIG', Path(self.temp.name) / 'settings.json'), \
                patch.object(launcher, 'launch', return_value=child):
            self.assertIsNone(launcher.main())
        child.wait.assert_not_called()

if __name__ == '__main__':
    unittest.main()
