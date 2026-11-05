# Table Tennis for Linux v1.3

Linux v1.3 of Mabec Games' fork of
[davidptrssvea's Windows recompilation](https://github.com/davidptrssvea/Table-Tennis-Rockstar-PC).

**Pressing Escape immediately quits the game, without a confirmation dialog (or closes active PC Settings / Achievements overlays).**

## What's new in v1.3

- **Rockstar-Themed Achievements Panel & Synthesized Chimes:**
  - Accessible via **EXTRAS > ACHIEVEMENTS** in the main menu, by holding the controller **View** button, or pressing **F7**.
  - Full gamepad navigation (**D-Pad / Left Stick** to scroll, **LB / RB** to page up/down, **B button** to exit) and keyboard support.
  - Live unlock toast notifications and synthesized unlock chime audio.
  - Background game and menu inputs are swallowed while open to prevent accidental selections.
- **In-Game PC Settings Menu:**
  - Open **OPTIONS > PC SETTINGS** from the pause menu or frontend options.
  - Full Xbox / PlayStation controller and mouse navigation.
  - Real-time resolution changing, fullscreen toggle, anti-aliasing (FXAA / FXAA Extreme), scaling/sharpening (FSR / CAS / Bilinear), picture aspect locks, and button prompts.
  - Settings are saved directly to `settings.ini`.
- **Dynamic Controller Button Prompts (Xbox & PlayStation):**
  - Dynamically detects SDL3 controllers and updates in-memory DXT5 textures to display native PlayStation or Xbox button prompts throughout menus and tutorials.
- **Ultrawide & Custom Aspect Ratio Support (21:9 & 32:9):**
  - Proper FOV adjustment for ultrawide displays.
  - 16:9 safe-area menu scissor clipping ensures menus and HUD elements preserve original proportions.
- **Photo Mode with 0.5x–5.0x Optical Zoom:**
  - Enter Photo Mode by pressing **F9** or clicking both thumbsticks in offline matches.
  - Full 3D camera controls with roll, variable speed, and optical zoom via Page Up/Down, mouse wheel, or D-Pad.
  - Screenshots are saved directly to the `photos/` directory in lossless PNG.
- **Microsecond POSIX Frame Pacer:**
  - Dedicated Linux POSIX `nanosleep` frame pacing at 60 FPS with microsecond spin recovery to eliminate presentation stutter.
- **In-Game GitHub Release Update Checker:**
  - Directly checks `Mabec-games/Table-Tennis-Rockstar-Linux` releases via **OPTIONS > PC SETTINGS > UPDATES** and opens releases in the default web browser via `xdg-open`.
- **Quit to Desktop & Cursor Auto-Hide:**
  - Added **QUIT TO DESKTOP** option to the main menu.
  - Cursor automatically hides after 2 seconds of inactivity.
- **Direct System Link Networking:**
  - Direct IP-based system link play supported across local networks and VPNs.
- **Includes all v1.2 & v1.1 performance updates:**
  - Radix user-space page table memory tracking, 2-character match GPU spin optimization, and `800p (Steam Deck)` preset.

## Download and build

- **`Table-Tennis-Linux-v1.3-x86_64.AppImage`** — Linux launcher and native game.
- **`Table-Tennis-Linux-v1.3-x86_64.AppImage.sha256`** — AppImage checksum.
- **`Table-Tennis-Linux-Source-v1.3.zip`** — integration source, tools, tests, documentation and presentation images.
- **`SHA256SUMS.txt`** — checksums for the AppImage and source ZIP.

Download the AppImage, make it executable and run it:

```bash
chmod +x Table-Tennis-Linux-v1.3-x86_64.AppImage
./Table-Tennis-Linux-v1.3-x86_64.AppImage
```

Select your own supported retail USA/Europe Xbox 360 ISO and press Play.
**No SDK, Python, Tk or compiler installation is required to play.**
The AppImage contains the native executable, patched menu override and runtime
dependencies. The ISO and XEX are not bundled.

For source builds, follow the
[Linux build guide](https://github.com/Mabec-games/Table-Tennis-Rockstar-Linux/blob/linux-launcher/LINUX.md).
The SDK is a build-time dependency only.

## Compatibility

The current AppImage packaging targets SteamOS 3.5+ (Steam Deck), current Arch,
Debian 13+ and Ubuntu 24.04+, with an x86-64-v3 CPU, Linux 6.1+ and a working
Vulkan driver. Debian 12 and Ubuntu 22.04 are outside this build's target.

On Steam Deck, select **`800p (Steam Deck)`** and **60 FPS**. On desktop/laptop
PCs, start with **1080p** and **60 FPS**. Validated on CachyOS across NVIDIA
RTX 3080 and AMD Radeon 680M (RADV, RDNA 2).

Use tag **`linux-v1.3`** to distinguish this release from upstream tags.
