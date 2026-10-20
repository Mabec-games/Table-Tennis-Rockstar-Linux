"""Run the bundled launcher with a writable, versioned game directory."""
import ctypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def stage_runtime(bundle, cache):
    manifest = json.loads((bundle / 'runtime-manifest.json').read_text())
    identity = hashlib.sha256(
        json.dumps(manifest, sort_keys=True).encode()).hexdigest()[:20]
    destination = cache / identity
    cache.mkdir(parents=True, exist_ok=True)
    # Keep the image immutable and let the native game's existing paths work,
    # including its logs and optional in-game settings file. Saves retain the
    # SDK's normal XDG_DATA_HOME/tabletennis location, shared with source runs.
    if not destination.is_dir():
        temporary = Path(tempfile.mkdtemp(prefix='.stage-', dir=cache))
        try:
            for relative, expected in manifest.items():
                source = bundle / 'build/linux' / relative
                if hashlib.sha256(source.read_bytes()).hexdigest() != expected:
                    raise ValueError(f'Bundled runtime checksum mismatch: {relative}')
                target = temporary / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, target)
            try:
                temporary.rename(destination)
            except OSError:
                if not destination.is_dir():
                    raise
        finally:
            if temporary.exists():
                shutil.rmtree(temporary)
    return destination / 'tabletennis'


def self_test():
    import math
    import tkinter
    import launcher_linux
    sys.path.insert(0, str(ROOT / 'project/tools'))
    import disc_image
    import patch_menu
    interpreter = tkinter.Tcl()
    plugin = ctypes.CDLL(str(ROOT / 'build/linux/librexgpu-xenos.so'))
    plugin.rex_gpu_abi_version.restype = ctypes.c_uint32
    if plugin.rex_gpu_abi_version() != 1:
        raise ValueError('Unsupported graphics plugin ABI')
    if math.cosh(0.0) != 1.0 or math.sinh(0.0) != 0.0:
        raise ValueError('Bundled Python math runtime failed')
    print(json.dumps({
        'python': sys.version.split()[0],
        'python_prefix': sys.prefix,
        'tcl': interpreter.eval('info patchlevel'),
        'tk_import': 'ok',
        'python_math': 'ok',
        'graphics_plugin_abi': 1,
        'iso_validation_import': 'ok',
        'game': str(ROOT / 'build/linux/tabletennis'),
    }, indent=2))


def main():
    if sys.argv[1:] == ['--self-test']:
        self_test()
        return 0
    from launcher_linux import main as launcher_main
    cache = Path(os.environ.get('XDG_CACHE_HOME', Path.home() / '.cache'))
    game = stage_runtime(ROOT, cache / 'tabletennis/appimage')
    sys.argv[1:1] = ['--game', str(game), '--wait']
    return launcher_main()


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, ImportError) as error:
        print(f'ERROR: {error}', file=sys.stderr)
        sys.exit(1)
