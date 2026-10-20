"""Experimental native Linux build using an installed ReXGlue 0.10.0 SDK."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

from disc_image import validate, extract
from patch_menu import patch as patch_menu
from patch_native import patch as patch_native

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--iso', type=Path, required=True)
    parser.add_argument('--sdk', type=Path, help='Installed Linux SDK prefix')
    parser.add_argument('--validate-iso', action='store_true')
    parser.add_argument('--prepare-only', action='store_true')
    parser.add_argument('--jobs', type=int, default=min(8, os.cpu_count() or 2))
    args = parser.parse_args()
    if not sys.platform.startswith('linux'):
        parser.error('Use setup_build.py on Windows')
    if not 1 <= args.jobs <= 32:
        parser.error('--jobs must be between 1 and 32')
    iso = args.iso.resolve(strict=True)
    if iso.suffix.lower() != '.iso':
        parser.error('Select a retail Xbox 360 ISO')
    validate(iso)
    if args.validate_iso:
        return
    if not args.sdk:
        parser.error('Supply --sdk with an installed Linux ReXGlue SDK v0.10.0; see LINUX.md')
    sdk = args.sdk.resolve(strict=True)
    codegen = sdk / 'bin/rexglue'
    version = subprocess.check_output([str(codegen), '--version'], text=True).strip()
    if version != '0.10.0':
        raise ValueError('This project requires SDK 0.10.0; generated edits are version-specific')
    if not (sdk / 'lib/cmake/rexglue').is_dir():
        raise ValueError('SDK prefix must include lib/cmake/rexglue')
    if not args.prepare_only:
        for tool in ('cmake', 'clang++', 'clang'):
            if not shutil.which(tool):
                raise ValueError(f'Missing build tool: {tool}')
        generator = 'Ninja' if shutil.which('ninja') else 'Unix Makefiles'
        if generator == 'Unix Makefiles' and not shutil.which('make'):
            raise ValueError('Install Ninja or Make')
    extract(iso, ROOT)
    patch_menu(ROOT / 'dist/assets/resource/flash/flash.rpf')
    subprocess.run([str(codegen), 'codegen', str(ROOT / 'project/tabletennis_manifest.toml')],
                   cwd=ROOT, check=True)
    patch_native(ROOT / 'project/generated/default')
    if args.prepare_only:
        return
    build = ROOT / 'build/linux'
    subprocess.run(['cmake', '-S', str(ROOT / 'project'), '-B', str(build), '-G', generator,
                    '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_C_COMPILER=clang',
                    '-DCMAKE_CXX_COMPILER=clang++', f'-DCMAKE_PREFIX_PATH={sdk}'], check=True)
    subprocess.run(['cmake', '--build', str(build), '--parallel', str(args.jobs)], check=True)
    shutil.copytree(ROOT / 'dist/assets', build / 'assets', dirs_exist_ok=True)
    # Run from the build tree to retain SDK library/plugin paths until packaging
    # has been validated against the actual Linux SDK.
    print(f'Built {build / "tabletennis"}. Launch with launch-linux.sh --game {build / "tabletennis"}')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f'ERROR: {error}', file=sys.stderr)
        sys.exit(1)
