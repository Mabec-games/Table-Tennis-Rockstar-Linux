"""Record guest CPU samples using saved launcher settings, then resolve them."""
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import os
import re
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def elf_address(path, offset):
    """Translate a mapped file offset to the corresponding ELF virtual address."""
    with path.open('rb') as file:
        header = file.read(64)
        if header[:6] != b'\x7fELF\x02\x01':
            raise ValueError(f'Expected little-endian ELF64: {path}')
        phoff = struct.unpack_from('<Q', header, 32)[0]
        phsize, phcount = struct.unpack_from('<HH', header, 54)
        file.seek(phoff)
        for _ in range(phcount):
            ph = file.read(phsize)
            kind, flags, start, address, _, filesz, _, _ = struct.unpack_from('<II6Q', ph)
            if kind == 1 and start <= offset < start + filesz:
                return offset - start + address
    raise ValueError(f'No load segment for {path} offset {offset:x}')


def analyse(path):
    counts, all_counts = Counter(), Counter()
    caller_counts = defaultdict(Counter)
    mappings = {}
    windows = []
    slow = False
    measurements = defaultdict(lambda: [0, 0.0])
    for line in path.read_text().splitlines():
        if line.startswith('WINDOW '):
            _, fps, cpu, size = line.split()
            fps, cpu = float(fps), float(cpu)
            slow = fps < 55 and cpu > 8
            windows.append((fps, cpu, int(size), slow))
        elif line.startswith('PC '):
            pc = int(line[3:], 16)
            all_counts[pc] += 1
            if slow:
                counts[pc] += 1
        elif line.startswith('RET'):
            fields = line[3:].split()
            if len(fields) == 1:  # Files from the first caller-capture version.
                depth, caller_text = 0, fields[0]
            else:
                depth, caller_text = int(fields[0]), fields[1]
            caller = int(caller_text, 16)
            if slow:
                caller_counts[depth][caller] += 1
        elif line.startswith('MAP '):
            match = re.match(r'MAP ([0-9a-f]+)-([0-9a-f]+) (\S+) ([0-9a-f]+) \S+ \S+\s+(/.*)', line)
            if match and 'x' in match[3]:
                start, end, offset = (int(match[i], 16) for i in (1, 2, 4))
                mappings[start, end] = (offset, Path(match[5]))
        elif line.startswith('MEASURE '):
            _, event, count, milliseconds = line.split()
            measurements[int(event)][0] += int(count)
            measurements[int(event)][1] += float(milliseconds)
    if not all_counts:
        raise ValueError('No CPU samples recorded; play long enough for a timing window')
    used_slow = bool(counts)
    if not used_slow:
        counts = all_counts
    grouped = defaultdict(list)
    unknown = 0
    for pc, count in counts.items():
        for (start, end), (offset, binary) in mappings.items():
            if start <= pc < end and binary.is_file():
                grouped[binary].append((pc - start + offset, count))
                break
        else:
            unknown += count
    functions, locations = Counter(), Counter()
    for binary, items in grouped.items():
        addresses = [elf_address(binary, offset) for offset, _ in items]
        result = subprocess.run(['addr2line', '-f', '-C', '-e', str(binary)],
                                input=''.join(f'{address:x}\n' for address in addresses),
                                text=True, capture_output=True, check=True)
        lines = result.stdout.splitlines()
        if len(lines) != len(items) * 2:
            raise ValueError(f'Unexpected addr2line output for {binary}')
        for i, (_, count) in enumerate(items):
            name, source = lines[i * 2:i * 2 + 2]
            functions[binary.name, name] += count
            locations[binary.name, name, source] += count
    total = sum(counts.values())
    graphics_worker = path.name.startswith('graphics-cpu-')
    report = [f'Profile: {path}',
              ('Scope: graphics command-processing worker CPU samples' if graphics_worker else
               'Scope: slow CPU-heavy guest windows' if used_slow else
               'Scope: all guest windows (no slow CPU-heavy windows found)'),
              f'Samples: {total}; unresolved mappings: {unknown}', '']
    if graphics_worker:
        report += ['Worker sample windows: ' + str(len(windows))]
    else:
        report += ['Timing windows: guest swaps/s | guest CPU ms/frame | samples | selected']
        report += [f'{fps:6.1f} | {cpu:7.2f} | {size:4d} | {slow}' for fps, cpu, size, slow in windows]
    report += ['', 'Top functions (exclusive CPU samples):']
    report += [f'{count / total:7.2%} {count:5d} {binary}: {name}'
               for (binary, name), count in functions.most_common(40)]
    report += ['', 'Top source locations:']
    report += [f'{count / total:7.2%} {count:5d} {binary}: {name} at {source}'
               for (binary, name, source), count in locations.most_common(60)]
    for depth, sampled_callers in sorted(caller_counts.items()):
        caller_groups = defaultdict(list)
        caller_unknown = 0
        for caller, count in sampled_callers.items():
            # Return addresses point just past the call instruction.
            caller -= 1
            for (start, end), (offset, binary) in mappings.items():
                if start <= caller < end and binary.is_file():
                    caller_groups[binary].append((caller - start + offset, count))
                    break
            else:
                caller_unknown += count
        caller_functions = Counter()
        caller_locations = Counter()
        for binary, items in caller_groups.items():
            addresses = [elf_address(binary, offset) for offset, _ in items]
            result = subprocess.run(['addr2line', '-f', '-C', '-e', str(binary)],
                                    input=''.join(f'{address:x}\n' for address in addresses),
                                    text=True, capture_output=True, check=True)
            lines = result.stdout.splitlines()
            if len(lines) != len(items) * 2:
                raise ValueError(f'Unexpected addr2line output for {binary}')
            for i, (_, count) in enumerate(items):
                name, source = lines[i * 2:i * 2 + 2]
                caller_functions[binary.name, name] += count
                caller_locations[binary.name, name, source] += count
        caller_total = sum(sampled_callers.values())
        report += ['', f'Stack slot {depth} return addresses (unresolved samples: {caller_unknown}):']
        report += [f'{count / caller_total:7.2%} {count:5d} {binary}: {name}'
                   for (binary, name), count in caller_functions.most_common(15)]
        report += ['', f'Stack slot {depth} source locations:']
        report += [f'{count / caller_total:7.2%} {count:5d} {binary}: {name} at {source}'
                   for (binary, name, source), count in caller_locations.most_common(20)]
    if measurements:
        report += ['', 'Graphics worker wall-time scopes (event: count, accumulated milliseconds):',
                   '1: packet WAIT_REG_MEM; 2: Vulkan blocking submission-fence wait; 3: idle command-worker wait']
        report += [f'{event}: {count}, {milliseconds:.3f} ms'
                   for event, (count, milliseconds) in sorted(measurements.items())]
    destination = path.with_suffix('.report.txt')
    destination.write_text('\n'.join(report) + '\n')
    print(f'CPU report saved: {destination}', flush=True)
    return destination


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--analyse', type=Path, help='Resolve an existing recording without launching')
    parser.add_argument('--resolution', choices=('720p', '800p', '800p (Steam Deck)', '1080p', '1440p', '4K'),
                        help='Override resolution for this run only')
    parser.add_argument('--aa', choices=('none', 'fxaa', 'fxaa_extreme'),
                        help='Override anti-aliasing for this run only')
    args = parser.parse_args()
    if args.analyse:
        analyse(args.analyse)
        return
    sys.path.insert(0, str(ROOT / 'project/launcher'))
    import launcher_linux as launcher
    settings = launcher.DEFAULTS.copy()
    if launcher.CONFIG.exists():
        saved = json.loads(launcher.CONFIG.read_text())
        settings.update({key: saved[key] for key in settings if key in saved})
    if args.resolution:
        settings['resolution'] = args.resolution
    settings['resolution'] = launcher.normalize_resolution(settings['resolution'])
    if args.aa:
        settings['aa'] = args.aa
    os.environ['TT_CPU_PROFILE'] = '1'
    # The system RenderDoc Vulkan layer is opt-in. A shell started from a
    # capture session may export this and distort the very timings we measure.
    os.environ.pop('ENABLE_VULKAN_RENDERDOC_CAPTURE', None)
    # Also apply the manifest's explicit disable switch and remove explicit
    # loader selections, which take precedence over merely clearing opt-in.
    os.environ['DISABLE_VULKAN_RENDERDOC_CAPTURE_1_46'] = '1'
    for variable in ('VK_INSTANCE_LAYERS', 'VK_LOADER_LAYERS_ENABLE'):
        layers = os.environ.get(variable, '')
        os.environ[variable] = ':'.join(
            layer for layer in layers.split(':') if layer != 'VK_LAYER_RENDERDOC_Capture')
    os.environ.setdefault('SDL_VIDEODRIVER', 'x11')
    print('Starting the game with saved settings and built-in CPU recording.\n'
          f'Resolution: {settings["resolution"]}; GPU: {settings["gpu"]}; AA: {settings["aa"]}.\n'
          'Play a table rally for about 20 seconds, then close the game normally.\n'
          'The CPU report will be written automatically.', flush=True)
    game = ROOT / 'build/linux/tabletennis'
    process = launcher.launch(game, settings)
    status = process.wait()
    paths = sorted((game.parent / 'logs').glob(f'*-cpu-{process.pid}-*.txt'))
    paths = [path for path in paths if path.stat().st_size]
    if not paths:
        raise ValueError(f'No recording produced (game exit {status}); inspect the latest game log')
    for path in paths:
        analyse(path)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f'CPU recording failed: {error}', file=sys.stderr)
        sys.exit(1)
