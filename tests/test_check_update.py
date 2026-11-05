"""Unit tests for Linux update check tool."""
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from project.tools import check_update


class TestCheckUpdate(unittest.TestCase):
    def test_parse_version(self):
        self.assertEqual(check_update.parse_version("1.2"), ([1, 2], "1.2"))
        self.assertEqual(check_update.parse_version("linux-v1.1"), ([1, 1], "1.1"))
        self.assertEqual(check_update.parse_version("Table-Tennis-Linux-v1.2-x86_64.AppImage"), ([1, 2], "1.2"))
        self.assertEqual(check_update.parse_version("invalid"), ([0], "0.0"))

    @patch('project.tools.check_update.fetch_releases')
    def test_run_check_up_to_date(self, mock_fetch):
        mock_fetch.return_value = [
            {
                "tag_name": "linux-v1.1",
                "html_url": "https://github.com/Mabec-games/Table-Tennis-Rockstar-Linux/releases/tag/linux-v1.1",
                "assets": [
                    {"name": "Table-Tennis-Linux-v1.2-x86_64.AppImage"}
                ]
            }
        ]
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / "status.txt"
            res = check_update.run_check(installed="1.2", output_path=output)
            self.assertEqual(res["state"], "up_to_date")
            self.assertEqual(res["version"], "1.2")
            self.assertTrue(output.exists())
            content = output.read_text()
            self.assertIn("state=up_to_date", content)
            self.assertIn("version=1.2", content)

    @patch('project.tools.check_update.fetch_releases')
    def test_run_check_available(self, mock_fetch):
        mock_fetch.return_value = [
            {
                "tag_name": "linux-v1.3",
                "html_url": "https://github.com/Mabec-games/Table-Tennis-Rockstar-Linux/releases/tag/linux-v1.3",
                "assets": [
                    {"name": "Table-Tennis-Linux-v1.3-x86_64.AppImage"}
                ]
            }
        ]
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / "status.txt"
            res = check_update.run_check(installed="1.2", output_path=output)
            self.assertEqual(res["state"], "available")
            self.assertEqual(res["version"], "1.3")
            self.assertTrue(output.exists())
            content = output.read_text()
            self.assertIn("state=available", content)
            self.assertIn("version=1.3", content)

    @patch('project.tools.check_update.fetch_releases')
    def test_run_check_failed(self, mock_fetch):
        mock_fetch.return_value = None
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / "status.txt"
            res = check_update.run_check(installed="1.2", output_path=output)
            self.assertEqual(res["state"], "failed")
            self.assertTrue(output.exists())
            content = output.read_text()
            self.assertIn("state=failed", content)


if __name__ == '__main__':
    unittest.main()
