"""Linux settings launcher for a locally built native game."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
CONFIG = Path(os.environ.get('XDG_CONFIG_HOME', Path.home() / '.config')) / 'tabletennis/settings.json'
PRESETS = {'720p': (1280, 720, 1), '1080p': (1920, 1080, 1),
           '1440p': (2560, 1440, 2), '4K': (3840, 2160, 3)}
DEFAULTS = dict(iso='', resolution='1080p', framerate='60', aa='none', post='fsr',
                fullscreen=False, show_fps=False, gpu='auto')


def arguments(settings):
    width, height, scale = PRESETS[settings['resolution']]
    if settings['framerate'] not in ('60', '120', '144', '240', '0'):
        raise ValueError('Unsupported framerate')
    if settings['aa'] not in ('none', 'fxaa', 'fxaa_extreme') or settings['post'] not in ('fsr', 'cas', 'bilinear'):
        raise ValueError('Unsupported image filter')
    iso = Path(settings['iso']).expanduser().resolve(strict=True)
    if not iso.is_file() or iso.suffix.lower() != '.iso':
        raise ValueError('Select a retail Xbox 360 ISO')
    return [f'--game_data_root={iso}', f'--resolution_scale={scale}',
            f'--draw_resolution_scale_x={scale}', f'--draw_resolution_scale_y={scale}',
            f'--window_width={width}', f'--window_height={height}',
            f'--fullscreen={str(settings["fullscreen"]).lower()}',
            f'--swap_post_effect={settings["aa"]}', f'--present_effect={settings["post"]}',
            f'--show_fps={str(settings["show_fps"]).lower()}',
            f'--framerate={settings["framerate"]}', '--protect_zero=false',
            '--gpu_allow_invalid_fetch_constants=true']


def enumerate_vulkan_devices(output):
    """Parse `vulkaninfo --summary` GPU indexes and names."""
    devices = []
    current_index = None
    current_type = None
    for line in output.splitlines():
        match = re.match(r'^\s*GPU(\d+):', line)
        if match:
            current_index = int(match.group(1))
            current_type = None
            continue
        match = re.match(r'^\s*deviceType\s*=\s*(.+?)\s*$', line)
        if match and current_index is not None:
            current_type = match.group(1)
            continue
        match = re.match(r'^\s*deviceName\s*=\s*(.+?)\s*$', line)
        if match and current_index is not None:
            devices.append((current_index, match.group(1), current_type))
            current_index = None
            current_type = None
    return devices


def available_vulkan_devices(environment=None):
    try:
        result = subprocess.run(['vulkaninfo', '--summary'], check=True, capture_output=True,
                                text=True, timeout=8, env=environment)
    except (OSError, subprocess.SubprocessError):
        return []
    return enumerate_vulkan_devices(result.stdout + '\n' + result.stderr)


def selected_device(settings, devices=None):
    value = str(settings.get('gpu', 'auto'))
    if value.isdecimal():
        return int(value)
    if devices is None:
        devices = available_vulkan_devices(os.environ.copy())
    if value.startswith('device:'):
        saved_name = value[len('device:'):]
        matching = next((index for index, name, _ in devices if name == saved_name), None)
        if matching is not None:
            return matching
    # Prefer discrete GPUs even when Vulkan reports an integrated GPU first.
    return next((index for index, _, device_type in devices
                 if device_type == 'PHYSICAL_DEVICE_TYPE_DISCRETE_GPU'), None)


def runtime_environment(game, environment=None):
    """Prefer libraries shipped beside the game over build-machine SDK paths."""
    result = dict(os.environ if environment is None else environment)
    bundled = str(game.expanduser().resolve().parent)
    existing = result.get('LD_LIBRARY_PATH', '')
    result['LD_LIBRARY_PATH'] = bundled + (os.pathsep + existing if existing else '')
    return result


def launch(game, settings, device=None, texture_workaround=False, readback=None, diagnose=False, render_target=None, diagnostic_gpu=False):
    game = game.expanduser().resolve(strict=True)
    if not game.is_file() or not os.access(game, os.X_OK):
        raise ValueError(f'Game is not executable: {game}')
    args = arguments(settings)
    if device is None:
        device = selected_device(settings)
    if device is not None:
        args.append(f'--vulkan_device={device}')
    if texture_workaround:
        args.append('--gpu_3d_to_2d_texture=true')
    if readback is not None:
        args.append(f'--readback_resolve={readback}')
    if diagnose:
        dump = ROOT / 'build/linux/shader-diagnostics'
        dump.mkdir(parents=True, exist_ok=True)
        args.extend([f'--dump_shaders={dump}', '--log_level=debug'])
    if render_target is not None:
        args.append(f'--render_target_path_vulkan={render_target}')
    if diagnostic_gpu:
        plugin = game.parent / 'librexgpu-xenos-diagnostics.so'
        if not plugin.is_file():
            raise ValueError('Diagnostic GPU plugin has not been staged')
        args.append('--gpu_plugin=xenos-diagnostics')
    # Reuse the upstream hash checks before starting the runtime.
    sys.path.insert(0, str(ROOT / 'project/tools'))
    from disc_image import validate
    validate(Path(settings['iso']).expanduser())
    log_dir = Path(os.environ.get('XDG_STATE_HOME', Path.home() / '.local/state')) / 'tabletennis'
    log_dir.mkdir(parents=True, exist_ok=True)
    with (log_dir / 'game.log').open('a', encoding='utf-8') as log:
        environment = runtime_environment(game)
        if environment.pop('TABLETENNIS_ALLOW_RENDERDOC', '') != '1':
            # Avoid an inherited global Vulkan capture layer in ordinary play.
            environment.pop('ENABLE_VULKAN_RENDERDOC_CAPTURE', None)
            environment['DISABLE_VULKAN_RENDERDOC_CAPTURE_1_46'] = '1'
            for variable in ('VK_INSTANCE_LAYERS', 'VK_LOADER_LAYERS_ENABLE'):
                layers = environment.get(variable, '')
                environment[variable] = ':'.join(
                    layer for layer in layers.split(':')
                    if layer != 'VK_LAYER_RENDERDOC_Capture')
        return subprocess.Popen([str(game), *args], cwd=game.parent, env=environment,
                                stdout=log, stderr=subprocess.STDOUT)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, default=ROOT / 'build/linux/tabletennis')
    parser.add_argument('--iso')
    parser.add_argument('--diagnostic-gpu', action='store_true',
                        help='Use the separately built diagnostic GPU plugin')
    parser.add_argument('--render-target-path', choices=('host', 'fsi'),
                        help='Temporary Vulkan render target implementation override')
    parser.add_argument('--diagnose-shaders', action='store_true',
                        help='Dump shaders and debug logs locally under ignored build output')
    parser.add_argument('--readback-resolve', choices=('none', 'some', 'full'),
                        help='Temporary GPU render-to-texture readback diagnostic')
    parser.add_argument('--resolution', choices=tuple(PRESETS), help='Override resolution for this launch')
    parser.add_argument('--vulkan-device', type=int, choices=range(16), help='Temporary Vulkan GPU index override')
    parser.add_argument('--texture-3d-workaround', action='store_true',
                        help='Test SDK compatibility wrappers for problematic 3D textures')
    parser.add_argument('--dry-run', action='store_true', help='Print arguments without starting a GUI or game')
    parser.add_argument('--play', action='store_true', help='Start using saved settings without the GUI')
    parser.add_argument('--wait', action='store_true',
                        help='Keep the launcher process alive until the game exits (AppImage)')
    args = parser.parse_args()
    settings = DEFAULTS.copy()
    if CONFIG.exists():
        saved = json.loads(CONFIG.read_text(encoding='utf-8'))
        settings.update({key: saved[key] for key in DEFAULTS if key in saved})
    if args.iso:
        settings['iso'] = args.iso
    if args.resolution:
        settings['resolution'] = args.resolution
    if args.vulkan_device is not None:
        settings['gpu'] = str(args.vulkan_device)
    if args.dry_run:
        command = [str(args.game), *arguments(settings)]
        if selected_device(settings) is not None:
            command.append(f'--vulkan_device={selected_device(settings)}')
        if args.texture_3d_workaround:
            command.append('--gpu_3d_to_2d_texture=true')
        if args.readback_resolve is not None:
            command.append(f'--readback_resolve={args.readback_resolve}')
        if args.diagnose_shaders:
            command.extend([f'--dump_shaders={ROOT / "build/linux/shader-diagnostics"}', '--log_level=debug'])
        if args.render_target_path is not None:
            command.append(f'--render_target_path_vulkan={args.render_target_path}')
        if args.diagnostic_gpu:
            command.append('--gpu_plugin=xenos-diagnostics')
        print(shlex.join(command))
        return
    if args.play:
        child = launch(args.game, settings, args.vulkan_device, args.texture_3d_workaround, args.readback_resolve, args.diagnose_shaders, args.render_target_path, args.diagnostic_gpu)
        return child.wait() if args.wait else None
    import tkinter as tk
    from tkinter import ttk, filedialog, messagebox
    window = tk.Tk()
    window.title('Table Tennis for Linux, 1.0')
    window.resizable(False, False)
    window.configure(background='#080d15')
    icon = tk.PhotoImage(file=str(Path(__file__).resolve().parent / 'assets/tux-icon.png'))
    window.iconphoto(True, icon)
    window._launcher_icon = icon
    variables = {}
    devices = available_vulkan_devices(os.environ.copy())
    gpu_labels = ['Auto · dedicated GPU preferred'] + [f'{index}: {name}' for index, name, _ in devices]
    gpu_values = {'Auto · dedicated GPU preferred': 'auto'}
    gpu_values.update({f'{index}: {name}': f'device:{name}' for index, name, _ in devices})
    gpu_value = str(settings.get('gpu', 'auto'))
    gpu_default = next((label for label, value in gpu_values.items() if value == gpu_value),
                       'Auto · dedicated GPU preferred')

    background_path = Path(__file__).resolve().parent / 'assets/table-tennis-background.png'
    try:
        original_background = tk.PhotoImage(file=str(background_path))
        # Tk supports integer zoom/subsample only; 2:3 gives this art a crisp,
        # compact backdrop without adding Pillow as a runtime dependency.
        background_image = original_background.zoom(2).subsample(3)
        width, height = background_image.width(), background_image.height()
    except tk.TclError:
        original_background = background_image = None
        width, height = 1024, 610

    canvas = tk.Canvas(window, width=width, height=height, highlightthickness=0,
                       borderwidth=0, background='#080d15')
    canvas.pack()
    if background_image:
        canvas.create_image(width // 2, height // 2, image=background_image, anchor='center')
        window._background_source = original_background
        window._background_image = background_image
        # A dithered overlay darkens the artwork while preserving its detail.
        canvas.create_rectangle(0, 0, width, height, fill='#07111d',
                                stipple='gray25', outline='')

    palette = {
        'panel': '#101a27', 'field': '#192839', 'text': '#f4f6f8',
        'muted': '#a9b6c5', 'accent': '#f5c400', 'line': '#39536d',
    }
    panel_width, panel_height = min(980, width - 52), min(560, height - 32)
    panel = tk.Canvas(canvas, width=panel_width, height=panel_height,
                      highlightthickness=1, highlightbackground=palette['line'],
                      borderwidth=0, background=palette['panel'])
    canvas.create_window(width // 2, height // 2, window=panel,
                         width=panel_width, height=panel_height)
    panel_background_path = Path(__file__).resolve().parent / 'assets/launcher-panel.png'
    try:
        panel_background = tk.PhotoImage(file=str(panel_background_path))
        panel.create_image(0, 0, image=panel_background, anchor='nw')
        panel._background_image = panel_background
    except tk.TclError:
        panel.create_rectangle(0, 0, panel_width, panel_height,
                               fill=palette['panel'], outline='')
    panel.create_rectangle(0, 0, panel_width, panel_height, outline=palette['line'])

    style = ttk.Style(window)
    style.theme_use('clam')
    style.configure('Launcher.TCombobox', fieldbackground=palette['field'],
                    background=palette['field'], foreground=palette['text'],
                    arrowcolor=palette['accent'], bordercolor=palette['line'],
                    padding=(9, 7))
    style.map('Launcher.TCombobox', fieldbackground=[('readonly', palette['field'])],
              foreground=[('readonly', palette['text'])],
              selectbackground=[('readonly', palette['field'])],
              selectforeground=[('readonly', palette['text'])])
    # Tk's readonly popup is a Listbox rather than part of the ttk style.
    # Set its palette separately so it matches the launcher fields.
    for option, value in (
            ('*TCombobox*Listbox.background', palette['field']),
            ('*TCombobox*Listbox.foreground', palette['text']),
            ('*TCombobox*Listbox.selectBackground', palette['line']),
            ('*TCombobox*Listbox.selectForeground', palette['text'])):
        window.option_add(option, value)
    def draw_text(x, y, text, color, font, anchor='nw'):
        panel.create_text(x, y, text=text, fill=color, font=font, anchor=anchor)

    draw_text(26, 20, 'ROCKSTAR GAMES PRESENTS', palette['muted'], ('Sans', 9, 'bold'))
    draw_text(26, 48, 'TABLE TENNIS', palette['text'], ('Sans', 23, 'bold'))
    draw_text(26, 97, 'Choose your setup and get straight to the table.',
              palette['muted'], ('Sans', 10))
    panel.create_rectangle(24, 128, panel_width - 24, 130,
                           fill=palette['accent'], outline='')
    draw_text(26, 151, 'GAME DISC IMAGE', palette['muted'], ('Sans', 8, 'bold'))
    variables['iso'] = tk.StringVar(value=settings['iso'])
    iso_entry = tk.Entry(panel, textvariable=variables['iso'], relief='flat', bd=0,
             bg=palette['field'], fg=palette['text'], insertbackground=palette['text'],
             highlightthickness=1, highlightbackground=palette['line'],
             highlightcolor=palette['accent'], font=('Sans', 10))
    panel.create_window(24, 183, window=iso_entry, width=panel_width - 170,
                        height=44, anchor='nw')

    def browse():
        selected = filedialog.askopenfilename(filetypes=[('Disc images', '*.iso *.ISO')])
        if selected:
            variables['iso'].set(selected)

    def make_button(parent, text, command, primary=False):
        return tk.Button(parent, text=text, command=command, relief='flat', bd=0,
                           padx=16, pady=8, cursor='hand2',
                           bg=palette['accent'] if primary else palette['field'],
                           fg='#161616' if primary else palette['text'],
                           activebackground='#ffdb36' if primary else palette['line'],
                           activeforeground='#101010' if primary else palette['text'],
                           font=('Sans', 10, 'bold'))

    panel.create_window(panel_width - 24, 183, window=make_button(panel, 'BROWSE', browse),
                        width=108, height=44, anchor='ne')

    panel.columnconfigure(0, weight=1)
    style.configure('Launcher.TCombobox', arrowsize=22,
                    padding=(12, 12))
    options = [
        ('resolution', 'DISPLAY RESOLUTION', tuple(PRESETS), settings['resolution']),
        ('gpu', 'GRAPHICS DEVICE', tuple(gpu_labels), gpu_default),
        ('framerate', 'FRAME-RATE LIMIT', ('60', '120', '144', '240', '0'), settings['framerate']),
        ('aa', 'ANTI-ALIASING', ('none', 'fxaa', 'fxaa_extreme'), settings['aa']),
        ('post', 'PRESENTATION FILTER', ('fsr', 'cas', 'bilinear'), settings['post']),
    ]
    for index, (key, label, choices, value) in enumerate(options):
        row, column = divmod(index, 2)
        x = 24 if column == 0 else panel_width // 2 + 12
        y_label = 230 + row * 78
        y_field = y_label + 20
        draw_text(x, y_label, label, palette['muted'], ('Sans', 8, 'bold'))
        variables[key] = tk.StringVar(value=value)
        combo = ttk.Combobox(panel, textvariable=variables[key], values=choices,
                             state='readonly', style='Launcher.TCombobox')
        panel.create_window(x, y_field, window=combo,
                            width=panel_width // 2 - 36, height=48, anchor='nw')

    for index, (key, label) in enumerate((('fullscreen', 'Fullscreen'),
                                          ('show_fps', 'Show FPS overlay'))):
        variables[key] = tk.BooleanVar(value=settings[key])
        x, y = panel_width // 2 + 12, 420 + index * 32

        def draw_checkbox(variable, label, x, y):
            panel.delete(f'checkbox-{label}')
            panel.create_rectangle(x, y, x + 20, y + 20, fill=palette['field'],
                                   outline=palette['line'], width=1,
                                   tags=(f'checkbox-{label}',))
            if variable.get():
                panel.create_line(x + 4, y + 10, x + 8, y + 15,
                                  x + 16, y + 5, fill=palette['accent'],
                                  width=3, capstyle='round', joinstyle='round',
                                  tags=(f'checkbox-{label}',))
            panel.create_text(x + 30, y + 10, text=label, anchor='w',
                              fill=palette['text'], font=('Sans', 9),
                              tags=(f'checkbox-{label}',))

        def toggle_checkbox(event, variable=variables[key], label=label, x=x, y=y):
            variable.set(not variable.get())
            draw_checkbox(variable, label, x, y)

        draw_checkbox(variables[key], label, x, y)
        panel.tag_bind(f'checkbox-{label}', '<Button-1>', toggle_checkbox)

    child = None

    def play():
        nonlocal child
        try:
            current = {key: value.get() for key, value in variables.items()}
            current['gpu'] = gpu_values[current['gpu']]
            child = launch(args.game, current, texture_workaround=args.texture_3d_workaround,
                           readback=args.readback_resolve, diagnose=args.diagnose_shaders,
                           render_target=args.render_target_path, diagnostic_gpu=args.diagnostic_gpu)
            CONFIG.parent.mkdir(parents=True, exist_ok=True)
            temporary = CONFIG.with_suffix('.tmp')
            temporary.write_text(json.dumps(current, indent=2) + '\n', encoding='utf-8')
            temporary.replace(CONFIG)
            window.destroy()
        except (OSError, ValueError, KeyError) as error:
            messagebox.showerror('Unable to launch', str(error))

    draw_text(26, panel_height - 44, 'Your settings are saved for next time.',
              palette['muted'], ('Sans', 9))
    panel.create_window(panel_width - 24, panel_height - 30,
                        window=make_button(panel, 'PLAY TABLE TENNIS  ›', play, primary=True),
                        anchor='se')

    window.mainloop()
    if args.wait and child is not None:
        return child.wait()


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, ImportError) as error:
        print(f'ERROR: {error}', file=sys.stderr)
        sys.exit(1)
