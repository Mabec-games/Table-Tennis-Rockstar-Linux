# Table Tennis for Linux v1.1 — build and runtime guide

Building the Linux game from source uses Python 3.10+ and the native ReXGlue
0.10.0 SDK. The launcher does not build the game and should not need an SDK
installation at runtime; the game's runtime library and graphics plugin must be
shipped beside its executable. The launcher uses system Python/Tkinter, and the
game uses the machine's Vulkan driver.
The Windows setup and WPF launcher remain available for Windows builds.
Native compilation succeeded on CachyOS x86_64 with Clang 23 and Make.
Boot and character visuals have been verified by the user. After the Linux
page-query optimization, a recorded rally averaged 59.96 guest frames/s at the
60 FPS cap. A separate AMD Radeon 680M run also completed gameplay at the 60 FPS
cap using RADV and native Wayland. Check guest timings when diagnosing speed:
the UI FPS counter alone can show 60 even when guest frames run slower.

## Linux v1.1 AppImage

`dist/Table-Tennis-Linux-v1.1-x86_64.AppImage` packages the native game, corrected
Xenos graphics plugin, ReXGlue runtime, launcher artwork, Python, Tcl/Tk, Vulkan
loader and GPU enumeration utility. Players do not install the SDK or Python/Tk.
Select your own supported retail ISO in the launcher; the ISO is not bundled.
**Pressing Escape immediately quits the game, without a confirmation dialog.**

On 2026-10-20, the user confirmed that the AppImage launched both the launcher
and game correctly, with no issues, on the CachyOS (Arch-based) test laptop.
Clean-machine and Debian/Ubuntu desktop testing remain pending.

This build targets SteamOS 3.5+ (Steam Deck), current Arch, Debian 13+, and
Ubuntu 24.04+ with an x86-64-v3 CPU, Linux 6.1+, and a working Vulkan driver.
Native game/library imports are checked against glibc 2.38. Python has a private
loader and libc, used only by the launcher; that libc is not inherited by the
game or its GPU drivers.
Debian 13 ships [glibc 2.41](https://packages.debian.org/en/trixie/libs/libc6);
Ubuntu 24.04 ships [glibc 2.39](https://packages.ubuntu.com/noble/libc6).
These requirements establish a compatibility target, not completed desktop
gameplay validation on either distribution. Debian 12 and Ubuntu 22.04 are
outside this build's compatibility target.

```bash
./dist/Table-Tennis-Linux-v1.1-x86_64.AppImage
# Headless runtime/import check:
./dist/Table-Tennis-Linux-v1.1-x86_64.AppImage --self-test
# If FUSE mounting is unavailable:
./dist/Table-Tennis-Linux-v1.1-x86_64.AppImage --appimage-extract-and-run
```

The launcher shares existing settings and saves with normal source launches.
It stages the native executable, runtime/plugin, and patched menu into
`$XDG_CACHE_HOME/tabletennis/appimage/<build-id>` (default `~/.cache`), so logs
and in-game configuration remain writable. Native logs are in that directory's
`logs/`; launcher output remains in `~/.local/state/tabletennis/game.log`.
The launcher waits for the game to exit, keeping bundled libraries available
for the game's entire lifetime.

Build with `python3 project/tools/build_appimage.py --appimagetool PATH
--runtime PATH --link-libm PATH`. Inputs are local copies of official
`appimagetool`, its x86_64 type-2 runtime, and an older `libm.so.6` (the local
build uses Debian 12's). It relinks the existing game and corrected production
plugin objects into separate packaging output, preserving the normal build.
Checksums and the ELF compatibility audit are saved in `build/appimage/`.
The packaging script currently consumes the local Arch/CachyOS Python/Tk
installation; this is a maintainer build prerequisite, not a player dependency.

The v1.1 AppImage is distributed as a separate GitHub release asset, alongside
checksums and a source archive. It contains the native executable and patched
menu override; it does not contain the ISO or XEX. Keep compiled binaries out
of Git history, and never include the ISO, captures, dumps or local saves.

## Get the SDK

Run `bash fetch-sdk-linux.sh` from a terminal with internet access. This downloads
the official Linux amd64 ZIP into ignored `.tools/` storage and verifies its
published SHA-256 before accepting it. No account authentication is required.

Source for the pinned artifact and checksum:
https://github.com/rexglue/rexglue-sdk/releases/expanded_assets/v0.10.0

Extract the verified archive into the standard local SDK directory:

```bash
unzip .tools/rexglue-sdk-0.10.0-linux-amd64.zip -d .tools/sdk-linux
chmod +x .tools/sdk-linux/linux-amd64/bin/rexglue
.tools/sdk-linux/linux-amd64/bin/rexglue --version
```

The last command must report `0.10.0`. Keep this standard path for the renderer
builder and AppImage packager. If the binary cannot load a required system
library, resolve that build-machine dependency before continuing.

Prepare the pinned public SDK source and glslang headers needed to compile the
corrected Vulkan plugin. Run this once in a fresh checkout:

```bash
git clone --no-checkout https://github.com/rexglue/rexglue-sdk.git .tools/rexglue-source
git -C .tools/rexglue-source checkout --detach f5337cdc947ff6d4c4196737e2c807a48f2a1fc2
git -C .tools/rexglue-source submodule update --init --depth 1 thirdparty/glslang
```

The expected glslang revision is
`f4f1d8a352ca1908943aea2ad8c54b39b4879080`. Only that submodule is needed by
the separate plugin builder; the prebuilt SDK supplies its linked libraries.

## Validate, build, launch

Keep the ISO outside the repository. Supply your own absolute path:

```bash
bash setup-linux.sh --iso '/path/to/Table Tennis.iso' --validate-iso
bash setup-linux.sh --iso '/path/to/Table Tennis.iso' --sdk "$PWD/.tools/sdk-linux/linux-amd64"
python3 project/tools/build_gpu_diagnostics.py --production
bash launch-linux.sh --iso '/path/to/Table Tennis.iso'
```

Build the corrected production plugin before playing. The stock SDK plugin
does not contain the character-skin fix. Subsequent executable rebuilds restore
the corrected plugin automatically when `build/gpu-fixed/out/` is present.

The build requires CMake, Ninja or Make, and a compatible Clang toolchain (upstream
requires Clang 19+). Install these through your distribution if missing.
`--prepare-only` performs local extraction, menu patching, code generation and
verified native patches without compiling. All game-derived output stays in
ignored `private_work/`, `dist/`, `project/generated/` and `build/` directories.

The launcher uses Tkinter. Install Python, Tkinter, and the optional Vulkan
device-list utility with the package manager:

```bash
# Debian 13 (Trixie)
sudo apt install python3 python3-tk vulkan-tools

# Arch Linux
sudo pacman -S --needed python tk vulkan-tools
```

The build also requires CMake, Ninja or Make, and Clang 19+. Debian 13's `clang`
package meets this version requirement; older Debian releases need a newer Clang
toolchain. Arch's rolling `clang` package is suitable. A Vulkan driver for the
machine's GPU is required to play; install the vendor's current driver package.
The GPU selector uses `vulkaninfo --summary` from `vulkan-tools`. If that utility
is unavailable or cannot enumerate devices, the launcher still offers Auto.
The SDK/source preparation commands also require Git, curl and unzip.

At launch, the Linux launcher prepends the selected game's directory to
`LD_LIBRARY_PATH`, so bundled SDK runtime libraries are resolved from the player
package rather than a developer's SDK directory. This removes the SDK install
path as a runtime dependency; it does not remove the SDK requirement for building
from source. The release AppImage bundles these dependencies; broader desktop
gameplay validation remains pending.

It stores settings in `$XDG_CONFIG_HOME/tabletennis/settings.json` (default
`~/.config`) and game logs in `$XDG_STATE_HOME/tabletennis/game.log` (default
`~/.local/state`). `--play` starts without a GUI using saved settings.
`--dry-run --iso PATH` prints the argument list without launching or saving.
`--game PATH` overrides the default `build/linux/tabletennis` executable.
The GUI's Vulkan GPU menu offers Auto and each detected device. A selected device
is saved by its reported name, then resolved to its current Vulkan index at launch
so enumeration changes do not silently redirect it. Auto prefers the first device
the Vulkan driver identifies as discrete, then falls back to driver order.
Selecting another device is useful for controlled diagnostics.

**Pressing Escape immediately quits the game, without a confirmation dialog.**

The launcher disables an inherited RenderDoc Vulkan capture layer for normal
play because it reduced gameplay performance on this system. To launch through
RenderDoc intentionally, set `TABLETENNIS_ALLOW_RENDERDOC=1` before starting
`launch-linux.sh`.

The Linux launcher smoke test runs on Debian 13 and Arch Linux in GitHub Actions.
It checks Python/Tkinter availability, shell syntax, launcher settings tests, and
a no-launch argument-generation run. This does not replace a gameplay test on each
distribution's GPU drivers and desktop environment.

The Linux executable accelerates ReXGlue's memory-management and
page-permission queries (`QueryProtect`, `Protect`, `AllocFixed`, `DeallocFixed`,
`MapFileView`, `UnmapFileView`) using a lock-free user-space radix page table
for all guest memory mappings, plus the kernel's `PROCMAP_QUERY` ioctl (on
Linux 6.11+) or a zero-allocation stack-buffer `/proc/self/maps` parser (on
Linux 6.1–6.10, including SteamOS 3.5/3.6 on Steam Deck) for untracked mappings.
This avoids scanning `/proc/self/maps` on each GPU write-watch fault and guest
heap commit regardless of kernel version. For a diagnostic comparison,
`TABLETENNIS_LEGACY_MEMORY_QUERY=1 bash launch-linux.sh` restores the original
SDK lookup without rebuilding. The integration checks run with
`python3 tests/test_linux_memory_query.py` against the installed Linux SDK.

On the local Ryzen 9 6900HS / Radeon 680M laptop, `bash profile-amd.sh` selects
the installed RADV Vulkan driver and reuses saved settings. A user-tested native
Wayland rally displayed correctly and sustained about 60 FPS. The corresponding
runtime log is `build/linux/logs/tabletennis_036.log`; CPU reports are
`graphics-cpu-35518-35528.report.txt` and `guest-cpu-35518-35562.report.txt`.
The same AMD-only run under X11/XWayland produced a black window because Vulkan
swapchain creation failed, while native Wayland created the swapchain. This is
specific to the tested hybrid laptop/configuration; it does not establish that
AMD requires Wayland generally. The regular launcher remains on its tested X11
default and selects the RTX 3080 first on this laptop.

## Validation status and remaining work

The local retail ISO passed upstream executable and menu archive hash checks.
Code generation and all verified native edits succeeded with the Linux SDK.
The native executable compiled and linked, and its linked shared libraries
resolve locally. The runtime and Xenos plugin are staged beside the executable.
Shell/Python syntax and 14 unit tests were checked (12 passed, two Windows-only
checks skipped). Startup in the assistant environment stops at SDL video
initialization with `No available video device`; test in a desktop terminal.
Subsequent desktop testing confirmed boot, character selection and table gameplay.
The skin shader correction fixed the black characters. The page-query optimization
then brought the recorded rally to 59.96 guest frames/s, with no measured timing
window above 60. This validates the tested session, not every gameplay scenario.

The native build uses the SDK's default graphics backend. The GPU selector and
launcher smoke tests run cross-distribution, while Vulkan driver behavior beyond
the tested NVIDIA/X11 and AMD/Wayland configurations still needs real hardware.
GUI appearance, audio, controllers, and saves also need broader validation.
The build tree receives the patched offline-menu assets beside the executable.
Packaging and desktop/Steam integration are subsequent steps.

Never commit the ISO, extracted assets, generated game code, or compiled game.
Only the versioned AppImage and reviewed source archive are release payloads;
do not upload other local build output. Review staged files before publishing.

## Confirmed skin rendering fix

The pinned SDK Vulkan translator reads texture result exponent bias from the
LOD word (fetch dword4) instead of dword3, making some skin samples 1/256 their
intended value. The diagnostic build's dword3 correction is visually confirmed.
Build and stage a normal plugin with only that correction:

```bash
python3 project/tools/build_gpu_diagnostics.py --production
bash launch-linux.sh
```

The first production build requires the pinned SDK source and glslang headers
described under “Get the SDK” above. CMake restores the corrected
plugin from `build/gpu-fixed/out/` after its normal SDK staging step when that
build is present, so later game rebuilds retain the fix. The original normal
plugin is preserved as `build/linux/librexgpu-xenos.before-exp-fix.so`. Restart
the game to load a newly staged plugin.
The Linux shell launcher defaults SDL to x11 and honors an explicit
`SDL_VIDEODRIVER`. The NVIDIA test selected the RTX 3080 and created its swapchain;
the Radeon test succeeded after selecting native Wayland. Diagnostic flags aren't
needed for normal play with the fixed production plugin. Broader hardware and
gameplay/save round-trip coverage remain open.

## Recording slow gameplay

To measure CPU work during a match using your saved launcher settings:

```bash
bash profile-gameplay.sh
```

Play at the table for about 20 seconds, then close the game normally. The command
writes reports for the guest thread and graphics command worker under
`build/linux/logs/`. It uses the game's optional Linux CPU sampler and
`addr2line`; no capture application is required. Reports include guest frame
rates, sampled CPU functions, and graphics worker time in command waits, Vulkan
fence waits, and idle waits. Normal launches do not enable sampling.
