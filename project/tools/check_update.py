#!/usr/bin/env python3
"""Check GitHub releases for Table Tennis Linux updates."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import urllib.request
import urllib.error

DEFAULT_REPO = "Mabec-games/Table-Tennis-Rockstar-Linux"
DEFAULT_INSTALLED = "1.3"


def parse_version(text):
    match = re.search(r'(\d+(?:\.\d+)+)', text or '')
    if match:
        parts = [int(p) for p in match.group(1).split('.')]
        return parts, match.group(1)
    return [0], '0.0'


def fetch_releases(repo):
    api_url = f"https://api.github.com/repos/{repo}/releases"
    headers = {"User-Agent": "TableTennis-Linux", "Accept": "application/vnd.github+json"}

    # Attempt 1: urllib
    try:
        req = urllib.request.Request(api_url, headers=headers)
        with urllib.request.urlopen(req, timeout=7) as resp:
            content = resp.read().decode('utf-8')
            return json.loads(content)
    except Exception:
        pass

    # Attempt 2: curl fallback
    try:
        proc = subprocess.run(
            ['curl', '-s', '--max-time', '7', '-H', 'User-Agent: TableTennis-Linux',
             '-H', 'Accept: application/vnd.github+json', api_url],
            capture_output=True, text=True, timeout=9
        )
        if proc.returncode == 0 and proc.stdout:
            return json.loads(proc.stdout)
    except Exception:
        pass

    return None


def run_check(repo=DEFAULT_REPO, installed=DEFAULT_INSTALLED, output_path=None):
    releases_url = f"https://github.com/{repo}/releases"
    data = fetch_releases(repo)

    if not data or not isinstance(data, list):
        msg = "Could not connect to GitHub"
        if isinstance(data, dict) and 'message' in data:
            msg = data['message']
        result = {
            "state": "failed",
            "version": installed,
            "url": releases_url,
            "message": msg
        }
        if output_path:
            write_status(output_path, result)
        return result

    latest_ver = [0]
    latest_str = ""
    latest_url = releases_url

    for item in data:
        tag = item.get("tag_name", "")
        item_url = item.get("html_url", releases_url)
        v, s = parse_version(tag)
        if v > latest_ver:
            latest_ver = v
            latest_str = s
            latest_url = item_url
        for asset in item.get("assets", []):
            name = asset.get("name", "")
            v_a, s_a = parse_version(name)
            if v_a > latest_ver:
                latest_ver = v_a
                latest_str = s_a
                latest_url = item_url

    installed_ver, _ = parse_version(installed)
    if latest_ver > installed_ver:
        state = "available"
    else:
        state = "up_to_date"

    result = {
        "state": state,
        "version": latest_str or installed,
        "url": latest_url,
        "message": ""
    }
    if output_path:
        write_status(output_path, result)
    return result


def write_status(path, result):
    dest = Path(path)
    dest.parent.mkdir(parents=True, exist_ok=True)
    temp = dest.with_name(dest.name + ".tmp")
    content = (
        f"state={result['state']}\n"
        f"version={result['version']}\n"
        f"url={result.get('url', '')}\n"
        f"message={result.get('message', '')}\n"
    )
    temp.write_text(content, encoding='utf-8')
    temp.replace(dest)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", default=DEFAULT_REPO, help="GitHub repository (owner/repo)")
    parser.add_argument("--installed", default=DEFAULT_INSTALLED, help="Currently installed version")
    parser.add_argument("--output", help="Path to write status.txt")
    parser.add_argument("--print", action="store_true", help="Print result")
    args = parser.parse_args()

    result = run_check(args.repo, args.installed, args.output)
    if getattr(args, 'print') or not args.output:
        print(f"state={result['state']}")
        print(f"version={result['version']}")
        print(f"url={result['url']}")
        if result['message']:
            print(f"message={result['message']}")
    return 0 if result['state'] != 'failed' else 1


if __name__ == "__main__":
    sys.exit(main())
