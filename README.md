![Mabec Games — Table Tennis for Linux](docs/images/mabec.png)

# Rockstar Games presents Table Tennis — Linux v1.0

**Native Linux port by Mabec Games, forked from
[davidptrssvea's Windows PC recompilation](https://github.com/davidptrssvea/Table-Tennis-Rockstar-PC).**

Bring your own Xbox 360 disc image and play with native Linux execution,
Vulkan rendering, a graphical settings launcher, and controller support.
The Windows project's recompilation, ISO support, offline menu and startup
improvements form the foundation of this fork. ReXGlue SDK v0.10.0 provides
the runtime and renderer.

**The Linux AppImage has been tested on CachyOS: both the launcher and game
started correctly with no issues reported.**

> **Pressing Escape immediately quits the game, without a confirmation dialog.**

## What's in Linux v1.0

- **Linux settings launcher** with saved ISO selection, GPU selection, resolution,
  frame-rate limit, fullscreen/windowed mode and FPS overlay.
- **Native Vulkan rendering**, including the correction for dark character skin.
- **Controller support and working in-game rumble**, confirmed during gameplay.
- **720p, 1080p, 1440p and 4K presets**, with FXAA/FXAA Extreme and
  FSR, CAS or bilinear presentation options.
- **Faster Linux memory-permission queries** on supported kernels, with a
  fallback for older kernels.
- **Local AppImage packaging**, bundling the game runtime, graphics plugin,
  Python, Tcl/Tk and launcher. Running that AppImage requires **no separate
  ReXGlue SDK, Python or Tk installation**.
- **Offline menu and intro improvements** inherited from the Windows project.

![Table Tennis gameplay on Linux](docs/images/SCREEN1.png)

## Getting started

This repository contains the integration source and local build tools. The
tested AppImage is a local build artifact; a prebuilt game AppImage is not
included in this source release. Build instructions are in [LINUX.md](LINUX.md).
The SDK and compiler are needed to **build** the game, not to **run** the
resulting AppImage.

You need your own unmodified **USA/Europe retail Xbox 360 Table Tennis ISO**
(Title ID `545407DF`). The build tools verify it and open it read-only. Keep
the ISO available: the game reads it during play.

Once you have built your AppImage:

```bash
chmod +x Table-Tennis-Linux-x86_64.AppImage
./Table-Tennis-Linux-x86_64.AppImage
```

Choose the ISO, set **1080p and 60 FPS** as a starting point, and press
**Play Table Tennis**. Existing launcher settings are reused.

If FUSE mounting is unavailable:

```bash
./Table-Tennis-Linux-x86_64.AppImage --appimage-extract-and-run
```

On the tested hybrid laptop, the Radeon 680M worked through native Wayland but
showed a black window through X11/XWayland. For that configuration:

```bash
SDL_VIDEODRIVER=wayland ./Table-Tennis-Linux-x86_64.AppImage
```

This is a result from one laptop, not a requirement for all AMD systems.

## Compatibility and testing

| Platform or configuration | Status |
| --- | --- |
| CachyOS, an Arch-based distribution | AppImage launcher and game confirmed working |
| NVIDIA RTX 3080 Laptop GPU, Ryzen 9 6900HS | Recorded 1080p gameplay at approximately 60 guest frames/s |
| AMD Radeon 680M, Ryzen 9 6900HS, native Wayland | Recorded 1080p gameplay near 60 guest frames/s, with a short dip in the sample |
| Other current Arch installations | Compatibility target; broader testing needed |
| Debian 13+ and Ubuntu 24.04+ | AppImage compatibility targets; desktop gameplay testing pending |

The current AppImage build targets **x86-64-v3 CPUs, Linux 6.1 or newer and a
working Vulkan graphics driver**. Debian 12 and Ubuntu 22.04 are outside this
build's target. Clean-machine testing remains open.

The 1440p and 4K presets increase internal rendering resolution. On the tested
laptop, 1440p ran noticeably slower than 1080p; turning off FXAA did not remove
that slowdown. Higher frame-rate choices are selectable limits, not verified
performance promises. Start at 1080p/60 FPS.

See [hardware validation](docs/linux-hardware-validation.md) for the evidence
and limitations. The repository includes Debian/Arch launcher smoke tests;
these do not replace desktop graphics testing.

## Controls and local files

| Control | Action |
| --- | --- |
| **Escape** | **Immediately quits the game — no confirmation dialog** |
| **F10** | Toggle the FPS/frame-time overlay |
| Gamepad | Gameplay controls, intro skipping and supported controller rumble |

If a first-run **Saved Game** prompt appears, answer it with the gamepad.

Launcher settings are stored in `~/.config/tabletennis/settings.json`.
Saves use `~/.local/share/tabletennis`. Launcher output goes to
`~/.local/state/tabletennis/game.log`; AppImage runtime logs are under
`~/.cache/tabletennis/appimage/<build-id>/logs/`. The corresponding XDG
environment variables override these defaults.

## Credits

- **Windows recompilation and original integration:** [davidptrssvea](https://github.com/davidptrssvea/Table-Tennis-Rockstar-PC).
  For the Windows version and installer, visit that upstream project.
- **Linux port, launcher presentation and testing:** Mabec Games.
- **Recompilation framework and runtime:** [ReXGlue](https://github.com/rexglue/rexglue-sdk).
- **Windowing, input and controller support:** SDL3.
- **Original Windows icon:** Gary aka POOTERMAN :) —
  [artwork credit](licenses/Icon/CREDITS.txt).
- Presentation artwork and the gameplay screenshot were supplied by Mabec Games.
  See [artwork credits](licenses/LinuxArtwork/CREDITS.md).

<p align="center">
  <img src="docs/images/TT_launcher.png" alt="Mabec Games Table Tennis Linux PC box artwork" width="360">
</p>

Project-authored integration and tools retain the [MIT license](LICENSE).
ReXGlue-derived files retain their [BSD notices](licenses/ReXGlue/LICENSE.txt);
the [SDL notice](licenses/SDL3/LICENSE.txt) is also retained. Artwork and
original game material are separate from the source-code license.

This is an independent project, unaffiliated with Rockstar Games or Take-Two.
The original game and its branding belong to their respective owners. Supply
your own game; no ISO, XEX, extracted retail archive, generated game functions,
or compiled game executable is included in this source release.
