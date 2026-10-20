"""Build the Linux v1.0 AppImage from local game objects and packaging tools.

No downloads or uploads. Requires an older libm at link time to avoid importing
the host's new sqrtf@GLIBC_2.43. Game libraries are checked against a glibc 2.38
ceiling (Debian 13 / Ubuntu 24.04 / current Arch targets). Python uses its own
private loader/libc so its extension modules do not raise the game's baseline.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import sysconfig

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / 'build/appimage'
APPDIR = WORK / 'TableTennis.AppDir'
PAYLOAD = APPDIR / 'usr/share/tabletennis'
LIB = APPDIR / 'usr/lib'
COPIED_SOURCES = set()
# Use the host's libc/loader and graphics/audio drivers. Bundling these can
# conflict with installed driver libraries. Everything else linked is bundled.
HOST_LIBS = re.compile(
    r'^(?:ld-linux.*|lib(?:c|m|dl|pthread|rt|resolv|util|anl)\.so(?:\..*)?)$')


def run(command, **kwargs):
    return subprocess.run([str(item) for item in command], check=True, **kwargs)


def copy(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    COPIED_SOURCES.add(Path(source).resolve())


def relink(directory, link_file, output, libm):
    command = shlex.split((directory / link_file).read_text())
    # CMake emits an escaped dollar for the shell; subprocess takes it literally.
    command = [item.replace(r'\$ORIGIN', '$ORIGIN') for item in command]
    command[command.index('-o') + 1] = str(output)
    command = [str(libm) if item == '-lm' else item for item in command]
    command = ['-Wl,-rpath,$ORIGIN' if item.startswith('-Wl,-rpath,') else item
               for item in command]
    # Do not overwrite CMake's normal incremental dependency file.
    command = [item for item in command if
               not item.startswith('--dependency-file=')]
    if '-Xlinker' in command:
        command.remove('-Xlinker')
    run(command, cwd=directory)


def dependencies(path):
    result = run(['ldd', path], capture_output=True, text=True)
    if 'not found' in result.stdout:
        raise ValueError(f'Missing dependencies for {path}:\n{result.stdout}')
    for line in result.stdout.splitlines():
        match = re.match(r'\s*(\S+) => (/\S+) \(', line)
        if match:
            name, source = match.groups()
            yield Path(name).name, source


def audit_elf(path):
    text = run(['readelf', '--version-info', path], capture_output=True, text=True).stdout
    versions = set(re.findall(r'GLIBC_(\d+\.\d+(?:\.\d+)?)', text))
    newer = [value for value in versions if tuple(map(int, value.split('.'))) > (2, 38)]
    if newer:
        raise ValueError(f'{path} requires glibc newer than 2.38: {newer}')
    return max(versions, key=lambda value: tuple(map(int, value.split('.'))), default='none')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--appimagetool', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--link-libm', type=Path, required=True)
    args = parser.parse_args()
    for path in (args.appimagetool, args.runtime, args.link_libm):
        if not path.is_file():
            raise ValueError(f'Missing build input: {path}')
    WORK.mkdir(parents=True, exist_ok=True)
    if APPDIR.exists():
        shutil.rmtree(APPDIR)
    game_dir = PAYLOAD / 'build/linux'
    game_dir.mkdir(parents=True)
    relink(ROOT / 'build/linux', Path('CMakeFiles/tabletennis.dir/link.txt'),
           game_dir / 'tabletennis', args.link_libm.resolve())
    relink(ROOT / 'build/gpu-fixed/out', Path('CMakeFiles/tt_gpu_diagnostics.dir/link.txt'),
           game_dir / 'librexgpu-xenos.so', args.link_libm.resolve())
    copy(ROOT / 'build/linux/librexruntime.so', game_dir / 'librexruntime.so')
    # Explicit allowlist: do not package the ISO, captures, logs, or user saves.
    copy(ROOT / 'build/linux/assets/resource/flash/flash.rpf',
         game_dir / 'assets/resource/flash/flash.rpf')
    for name in ('launcher_linux.py', 'appimage_entry.py'):
        copy(ROOT / 'project/launcher' / name, PAYLOAD / 'project/launcher' / name)
    shutil.copytree(ROOT / 'project/launcher/assets', PAYLOAD / 'project/launcher/assets')
    for name in ('disc_image.py', 'patch_menu.py'):
        copy(ROOT / 'project/tools' / name, PAYLOAD / 'project/tools' / name)
    manifest = {str(path.relative_to(game_dir)): hashlib.sha256(path.read_bytes()).hexdigest()
                for path in sorted(game_dir.rglob('*')) if path.is_file()}
    (PAYLOAD / 'runtime-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')

    copy(Path(sys.executable).resolve(), APPDIR / 'usr/bin/python3')
    copy(Path('/usr/bin/vulkaninfo'), APPDIR / 'usr/bin/vulkaninfo')
    copy(Path('/usr/lib/libvulkan.so.1'), LIB / 'libvulkan.so.1')
    # SDL loads these display libraries with dlopen, so ldd cannot discover them.
    display_libraries = (
        'libXcursor.so.1', 'libXi.so.6', 'libXfixes.so.3', 'libXrandr.so.2',
        'libXtst.so.6', 'libwayland-cursor.so.0', 'libwayland-egl.so.1',
        'libxkbcommon.so.0')
    for name in display_libraries:
        copy(Path('/usr/lib') / name, LIB / name)
    version = f'python{sys.version_info.major}.{sys.version_info.minor}'
    stdlib = Path(sysconfig.get_path('stdlib'))
    shutil.copytree(stdlib, LIB / version, ignore=shutil.ignore_patterns(
        '__pycache__', '*.pyc', 'site-packages', 'dist-packages', 'test', 'tests',
        'idlelib', 'ensurepip', 'config-*', '_test*', '_ctypes_test*',
        '_curses*', 'readline*.so', '_sqlite3*.so'))
    for name in ('tcl8', 'tcl8.6', 'tk8.6'):
        shutil.copytree(Path('/usr/lib') / name, LIB / name)
    # Isolate Python's libc from the game and host GPU drivers. Invoke this
    # loader with --library-path, never export this directory in LD_LIBRARY_PATH.
    python_runtime = APPDIR / 'usr/lib/python-runtime'
    for name in ('ld-linux-x86-64.so.2', 'libc.so.6', 'libm.so.6',
                 'libdl.so.2', 'libpthread.so.0', 'librt.so.1',
                 'libresolv.so.2', 'libutil.so.1'):
        source = Path('/usr/lib') / name
        if source.exists():
            copy(source, python_runtime / name)
    # Only the game and interpreter need a full dependency closure.
    candidates = [game_dir / name for name in
                  ('tabletennis', 'librexruntime.so', 'librexgpu-xenos.so')]
    candidates += [APPDIR / 'usr/bin/python3', APPDIR / 'usr/bin/vulkaninfo',
                   LIB / 'libvulkan.so.1']
    candidates += [LIB / name for name in display_libraries]
    candidates += list((LIB / version / 'lib-dynload').glob('*.so'))
    copied = {'librexruntime.so', 'librexgpu-xenos.so'}
    for candidate in candidates:
        for name, source in dependencies(candidate):
            if name not in copied and not HOST_LIBS.match(name):
                copy(Path(source), LIB / name)
                copied.add(name)
    licenses = APPDIR / 'usr/share/licenses/tabletennis'
    copy(ROOT / 'LICENSE', licenses / 'TableTennis-LICENSE')
    shutil.copytree(ROOT / 'licenses', licenses / 'project')
    copy(stdlib / 'LICENSE.txt', licenses / 'Python-LICENSE.txt')
    for name in ('tcl', 'tk'):
        shutil.copytree(Path('/usr/share/licenses') / name, licenses / name)
    sdk_license = ROOT / '.tools/rexglue-source/LICENSE'
    if sdk_license.exists():
        copy(sdk_license, licenses / 'ReXGlue-LICENSE')
    # Preserve notices for the local system libraries used in this build.
    if shutil.which('pacman'):
        packages = set()
        for source in list(COPIED_SOURCES):
            if str(source).startswith('/usr/'):
                result = subprocess.run(['pacman', '-Qqo', str(source)], text=True,
                                        capture_output=True)
                if result.returncode == 0:
                    packages.update(result.stdout.splitlines())
        for package in sorted(packages):
            notices = Path('/usr/share/licenses') / package
            if notices.is_dir():
                shutil.copytree(notices, licenses / 'system' / package, dirs_exist_ok=True)
        result = run(['pacman', '-Q', *sorted(packages)], capture_output=True, text=True)
        (licenses / 'system-packages.txt').write_text(result.stdout)
        spdx = Path('/usr/share/licenses/spdx')
        if spdx.is_dir():
            shutil.copytree(spdx, licenses / 'spdx')

    (APPDIR / 'AppRun').write_text('''#!/bin/sh
set -eu
APPDIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
export APPDIR
export PATH="$APPDIR/usr/bin:$PATH"
export PYTHONHOME="$APPDIR/usr"
unset PYTHONPATH
export PYTHONNOUSERSITE=1 PYTHONDONTWRITEBYTECODE=1
export TCL_LIBRARY="$APPDIR/usr/lib/tcl8.6"
export TK_LIBRARY="$APPDIR/usr/lib/tk8.6"
export LD_LIBRARY_PATH="$APPDIR/usr/share/tabletennis/build/linux:$APPDIR/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-x11}"
exec "$APPDIR/usr/lib/python-runtime/ld-linux-x86-64.so.2" \\
  --library-path "$APPDIR/usr/lib/python-runtime:$APPDIR/usr/lib" \\
  "$APPDIR/usr/bin/python3" -s "$APPDIR/usr/share/tabletennis/project/launcher/appimage_entry.py" "$@"
''')
    (APPDIR / 'AppRun').chmod(0o755)
    desktop = '''[Desktop Entry]
Type=Application
Name=Table Tennis for Linux
Comment=Table Tennis for Linux v1.0; bring your own retail disc image
Exec=AppRun
Icon=tabletennis
Categories=Game;SportsGame;
Terminal=false
'''
    (APPDIR / 'tabletennis.desktop').write_text(desktop)
    copy(ROOT / 'project/launcher/assets/tux-icon.png', APPDIR / 'tabletennis.png')
    (APPDIR / '.DirIcon').symlink_to('tabletennis.png')
    (APPDIR / 'README.txt').write_text('''Table Tennis for Linux v1.0 — Mabec Games

Linux fork of davidptrssvea/Table-Tennis-Rockstar-PC, built with ReXGlue 0.10.0.

Launch the AppImage and select your own retail USA/Europe Xbox 360 ISO.
No SDK, system Python, or system Tk installation is required.
Pressing Escape immediately quits the game, without a confirmation dialog.

Targets: current Arch, Debian 13+, Ubuntu 24.04+; x86-64-v3 CPU, kernel 6.1+,
and a working Vulkan graphics driver. Desktop gameplay on Debian/Ubuntu still
needs verification. Debian 12 and Ubuntu 22.04 are not supported by this build.

Settings: ~/.config/tabletennis/settings.json
Saves: ~/.local/share/tabletennis (existing SDK defaults)
Launcher log: ~/.local/state/tabletennis/game.log
Runtime logs: ~/.cache/tabletennis/appimage/<build-id>/logs/
The corresponding XDG variables override these locations.

Use --self-test for a headless check. Use --appimage-extract-and-run on systems
where FUSE mounting is unavailable. This AppImage includes the native compiled
game, patched menu override and runtime dependencies. No ISO or XEX is included.
Original game and branding belong to their respective owners. Project and
dependency notices are in usr/share/licenses/tabletennis.
''')
    run(['desktop-file-validate', APPDIR / 'tabletennis.desktop'])
    audit = {}
    for path in sorted(APPDIR.rglob('*')):
        if path.is_file():
            with path.open('rb') as stream:
                elf = stream.read(4) == b'\x7fELF'
            if elf:
                relative = str(path.relative_to(APPDIR))
                if path.is_relative_to(LIB / version) or path.is_relative_to(python_runtime):
                    audit[relative] = 'private Python libc (not inherited by game)'
                else:
                    audit[relative] = audit_elf(path)
    (WORK / 'glibc-audit.json').write_text(json.dumps(audit, indent=2) + '\n')
    (WORK / 'build-inputs.json').write_text(json.dumps({
        str(path): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in (args.appimagetool, args.runtime, args.link_libm)
    }, indent=2) + '\n')
    run([APPDIR / 'AppRun', '--self-test'])
    output = ROOT / 'dist/Table-Tennis-Linux-v1.0-x86_64.AppImage'
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix('.AppImage.new')
    run([args.appimagetool.resolve(), '--appimage-extract-and-run',
         '--runtime-file', args.runtime.resolve(), '--no-appstream',
         '--mksquashfs-opt', '-processors', '--mksquashfs-opt', '4',
         APPDIR, temporary], env=dict(os.environ, ARCH='x86_64'))
    temporary.replace(output)
    output.chmod(0o755)
    digest = hashlib.sha256(output.read_bytes()).hexdigest()
    output.with_suffix('.AppImage.sha256').write_text(f'{digest}  {output.name}\n')
    print(f'Built {output} ({output.stat().st_size / 1024 / 1024:.1f} MiB)')


if __name__ == '__main__':
    main()
