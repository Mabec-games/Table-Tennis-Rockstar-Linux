# Table Tennis for Linux v1.0

The first Linux version of Mabec Games' fork of
[davidptrssvea's Windows recompilation](https://github.com/davidptrssvea/Table-Tennis-Rockstar-PC).
It adds native Linux/Vulkan support, a graphical settings launcher and local
AppImage packaging while retaining upstream credit and the Windows source.

The Linux AppImage launcher and game have been confirmed working on CachyOS.
Controller input, audio and in-game rumble have also been confirmed during
Linux testing. Recorded 1080p sessions reached approximately 60 guest frames/s
on the tested RTX 3080 Laptop GPU and Radeon 680M configurations.

**Pressing Escape immediately quits the game, without a confirmation dialog.**

## Included

- Linux launcher with saved settings and GPU selection.
- Corrected Vulkan character-skin rendering.
- Linux memory-query optimization with a legacy fallback.
- Resolution and filter controls, fullscreen/windowed mode and FPS overlay.
- SDK-free AppImage runtime packaging, including Python and Tcl/Tk.
- Optional local profiling tools and Debian/Arch launcher smoke-test workflow.

## Download and build

- **`Table-Tennis-Linux-v1.0-x86_64.AppImage`** — Linux launcher and native game.
- **`Table-Tennis-Linux-v1.0-x86_64.AppImage.sha256`** — AppImage checksum.
- **`Table-Tennis-Linux-Source-v1.0.zip`** — integration source, tools, tests,
  documentation and presentation images.
- **`SHA256SUMS.txt`** — checksums for the AppImage and source ZIP.

Download the AppImage, make it executable and run it:

```bash
chmod +x Table-Tennis-Linux-v1.0-x86_64.AppImage
./Table-Tennis-Linux-v1.0-x86_64.AppImage
```

Select your own supported retail USA/Europe Xbox 360 ISO and press Play.
**No SDK, Python, Tk or compiler installation is required to play.**
The AppImage contains the native executable, patched menu override and runtime
dependencies. The ISO and XEX are not bundled.

For source builds, follow the
[Linux build guide](https://github.com/Mabec-games/Table-Tennis-Rockstar-Linux/blob/linux-launcher/LINUX.md).
The SDK is a build-time dependency only.

## Compatibility

The current AppImage packaging targets current Arch, Debian 13+ and Ubuntu
24.04+, with an x86-64-v3 CPU, Linux 6.1+ and a working Vulkan driver.
CachyOS desktop testing passed; Debian/Ubuntu desktop and clean-machine tests
remain pending. Debian 12 and Ubuntu 22.04 are outside this build's target.

Start with 1080p and 60 FPS. Higher internal resolutions reduced gameplay
speed on the tested laptop. The Radeon 680M test required native Wayland on
that hybrid-GPU setup. These observations are specific to the tested hardware.

Linux versioning starts at **v1.0**, independently of upstream Windows v1.0.5.
Use tag **`linux-v1.0`** to distinguish this release from upstream tags.
