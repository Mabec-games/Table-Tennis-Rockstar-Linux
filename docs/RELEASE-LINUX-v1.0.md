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

This is a **source release**. The source ZIP contains project tools, integration,
tests, documentation and presentation images. Build locally with your own
supported retail USA/Europe Xbox 360 ISO following [LINUX.md](../LINUX.md).

The SDK is required by the build tools. A locally built AppImage bundles its
runtime dependencies and does not require players to install an SDK or Python.
No prebuilt game AppImage, ISO, extracted game archive or generated game code is
attached to this release.

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
