![Mabec Games — Table Tennis for Linux](docs/images/mabec.png)

# Rockstar Games presents Table Tennis — Linux v1.3

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

## What's new in Linux v1.3

Linux v1.3 brings a major feature synchronization with the upstream PC recompilation (Windows v1.5.3), completely native Linux adaptations, in-game PC settings, and custom achievements:

- **Rockstar-Themed Achievements Panel & Sound Effects:**
  - Full in-game Table Tennis achievements UI accessible directly from **EXTRAS > ACHIEVEMENTS** on the main menu, by holding the controller **View** button, or pressing **F7**.
  - Fully navigable with gamepad (**D-Pad / Left Stick** to scroll, **LB / RB** to page up/down, **B button** to exit) and keyboard, with full background input swallowing.
  - In-game notification toasts on achievement unlock and synthesized achievement unlock audio chime.
  - Automatically loads and tracks all 29 achievements (1000 G).

![Achievements list in Table Tennis for Linux v1.3](docs/images/achievements.jpg)

- **In-Game PC Settings Menu:**
  - Accessible via **OPTIONS > PC SETTINGS** with full Xbox / PlayStation gamepad and mouse support.
  - Change display resolution on the fly, toggle fullscreen, switch anti-aliasing (FXAA / FXAA Extreme), configure post-processing scaling (FSR / CAS / Bilinear), and customize button prompts.
  - Settings are saved directly to `settings.ini` without modifying your saves or ISO.
- **Dynamic Controller Button Prompts (Xbox & PlayStation):**
  - Dynamically detects connected SDL3 controllers and updates in-memory DXT5 UI textures in real time to present native PlayStation (Cross, Circle, Square, Triangle) or Xbox (A, B, X, Y) button icons across all menus and tutorials.
- **Ultrawide & Custom Aspect Ratio Support (21:9 & 32:9):**
  - Native support for ultrawide displays with automatic horizontal FOV scaling.
  - Scissor-clipped 16:9 safe-area locks ensure legacy UI, menus, scoreboards, and character viewer preserve original proportions without stretching or letterboxing bugs.
- **Photo Mode with 0.5x–5x Optical Zoom:**
  - Enter Photo Mode by pressing **F9** or clicking both thumbsticks simultaneously in offline matches.
  - Free 3D camera controls with camera roll, variable speed, and **0.5x to 5.0x optical zoom** using Page Up / Page Down, mouse wheel, or D-Pad.
  - High-resolution screenshots saved automatically to the `photos/` folder as clean PNGs.
- **Microsecond POSIX Frame Pacer:**
  - Linux POSIX `nanosleep` frame pacing at a steady 60 FPS with microsecond spin recovery to eliminate stutter and presentation hitching.
- **In-Game GitHub Release Updates:**
  - Background update checker querying [Mabec-games/Table-Tennis-Rockstar-Linux](https://github.com/Mabec-games/Table-Tennis-Rockstar-Linux) directly.
  - Check status in **OPTIONS > PC SETTINGS > UPDATES** and open releases with one click via `xdg-open`.
- **Quit to Desktop & Cursor Auto-Hide:**
  - Added clean **QUIT TO DESKTOP** option on the main menu.
  - Mouse cursor automatically hides after 2 seconds of inactivity during gameplay and menus.
- **System Link Networking:**
  - Direct IP-based system link play supported across local networks and virtual LANs.

---

## What's in Linux v1.2 & v1.1

- **2-character match slowdown fixes for Steam Deck / constrained RDNA 2 APUs:**
  replaces 1–2+ ms mid-frame `WAIT_REG_MEM` sleeps in the Xenos GPU worker with
  a low-latency spin/yield, disables 256 KB shared-memory invalidation
  over-widening (nearly halving per-frame `memcpy` upload overhead on RADV),
  skips redundant `mprotect` syscalls via the user-space page table, and yields
  cleanly in the guest D3D9 ring-buffer wait (`sub_82359DC0`).
- **Kernel-independent O(1) guest memory tracking** across `QueryProtect`,
  `Protect`, `AllocFixed`, `DeallocFixed`, `MapFileView`, and `UnmapFileView`,
  eliminating `/proc/self/maps` slowdowns on Linux 6.1–6.10 kernels (including
  SteamOS 3.5/3.6 on Steam Deck and Ubuntu 24.04 GA) as well as Linux 6.11+.
- **Dedicated `800p (Steam Deck)` (`1280x800`) resolution preset** alongside
  **720p, 1080p, 1440p and 4K**, with FXAA/FXAA Extreme and FSR, CAS or
  bilinear presentation options.
- **Linux settings launcher** with saved ISO selection, GPU selection, resolution,
  frame-rate limit, fullscreen/windowed mode and FPS overlay.
- **Native Vulkan rendering**, including the correction for dark character skin.
- **Controller support and working in-game rumble**, confirmed during gameplay.
- **Local AppImage packaging**, bundling the game runtime, graphics plugin,
  Python, Tcl/Tk and launcher. Running that AppImage requires **no separate
  ReXGlue SDK, Python or Tk installation**.
- **Offline menu and intro improvements** inherited from the Windows project.

![Table Tennis gameplay on Linux](docs/images/SCREEN1.png)

## Getting started

**[Download the Linux v1.3 AppImage](https://github.com/Mabec-games/Table-Tennis-Rockstar-Linux/releases/download/linux-v1.3/Table-Tennis-Linux-v1.3-x86_64.AppImage)**
from the [Linux v1.3 release](https://github.com/Mabec-games/Table-Tennis-Rockstar-Linux/releases/tag/linux-v1.3).
The release also includes a SHA-256 checksum and source ZIP.

**Players do not need to install the SDK, Python, Tk or a compiler.**
The AppImage bundles the native executable, corrected renderer and runtime.
For developers building their own copy, see [LINUX.md](LINUX.md).

You need your own unmodified **USA/Europe retail Xbox 360 Table Tennis ISO**
(Title ID `545407DF`). The build tools verify it and open it read-only. Keep
the ISO available: the game reads it during play.

Make the downloaded AppImage executable and start it:

```bash
chmod +x Table-Tennis-Linux-v1.3-x86_64.AppImage
./Table-Tennis-Linux-v1.3-x86_64.AppImage
```

Choose the ISO, set **800p (Steam Deck)** on Steam Deck or **1080p** on desktop
with **60 FPS** as a starting point, and press **Play Table Tennis**. Existing
launcher settings are reused.

If FUSE mounting is unavailable:

```bash
./Table-Tennis-Linux-v1.3-x86_64.AppImage --appimage-extract-and-run
```

On the tested hybrid laptop, the Radeon 680M worked through native Wayland but
showed a black window through X11/XWayland. For that configuration:

```bash
SDL_VIDEODRIVER=wayland ./Table-Tennis-Linux-v1.3-x86_64.AppImage
```

This is a result from one laptop, not a requirement for all AMD systems.

## Compatibility and testing

| Platform or configuration | Status |
| --- | --- |
| CachyOS, an Arch-based distribution | AppImage launcher and game confirmed working |
| NVIDIA RTX 3080 Laptop GPU, Ryzen 9 6900HS | Recorded 800p and 1080p gameplay at 60.0 guest frames/s |
| AMD Radeon 680M (RADV, RDNA 2), Ryzen 9 6900HS, native Wayland | Recorded 1080p gameplay near 60 guest frames/s; core-constrained (`taskset -c 0-3`, 2 cores / 4 threads) 800p match rally improved to ~50–56 guest frames/s |
| SteamOS / Steam Deck (Linux 6.1/6.5) | User-space page table, `800p (Steam Deck)` preset, and v1.2 2-character match fixes verified on RDNA 2 / RADV; follow-up physical Steam Deck retest pending |
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

## Controls and shortcuts

| Control | Action |
| --- | --- |
| **Escape** | **Immediately quits the game — no confirmation dialog** (or closes Achievements / PC Settings overlay) |
| **F7** / **Hold View** | Open the in-game Achievements panel (navigable with D-pad / sticks, LB/RB page jump, B to close) |
| **F9** / **Click Both Sticks** | Toggle Photo Mode camera in offline matches |
| **Page Up / Down / Mouse Wheel / D-Pad** | Optical zoom in Photo Mode (0.5x–5x) |
| **F10** | Toggle the FPS/frame-time overlay |
| **Gamepad** | Full gameplay controls, in-game menu navigation, and supported controller rumble |

If a first-run **Saved Game** prompt appears, answer it with the gamepad.

Launcher settings are stored in `~/.config/tabletennis/settings.json`.
Saves use `~/.local/share/tabletennis`. Launcher output goes to
`~/.local/state/tabletennis/game.log`; AppImage runtime logs are under
`~/.cache/tabletennis/appimage/<build-id>/logs/`. The corresponding XDG
environment variables override these defaults.

## Credits

- **Windows recompilation and original integration:** [davidptrssvea](https://github.com/davidptrssvea/Table-Tennis-Rockstar-PC).
  For the Windows version and installer, visit that upstream project.
- **Achievements UI, online and System Link play, relay server
  (`server/ttlink_server.py`), PlayStation button prompts, PC SETTINGS menu
  and photo mode:** [ThisIsAkill/Table-Tennis-Linux](https://github.com/ThisIsAkill/Table-Tennis-Linux)
  (MIT), adapted through the Windows project. Thanks to **ThisIsAkill**!
  See the [notice](licenses/Table-Tennis-Linux/NOTICE.txt) and
  [license](licenses/Table-Tennis-Linux/LICENSE.txt).
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
the [SDL notice](licenses/SDL3/LICENSE.txt) is also retained, as is the
[MIT notice](licenses/Table-Tennis-Linux/NOTICE.txt) for code adapted from
ThisIsAkill/Table-Tennis-Linux. Artwork and
original game material are separate from the source-code license.

This is an independent project, unaffiliated with Rockstar Games or Take-Two.
The original game and its branding belong to their respective owners. Supply
your own game ISO; no ISO or XEX is included. The source repository contains
integration code and tools. The separate AppImage release contains the native
compiled game, a patched menu override and bundled runtime dependencies.
