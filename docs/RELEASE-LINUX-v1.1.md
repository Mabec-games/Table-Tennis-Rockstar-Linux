# Table Tennis for Linux v1.1

Linux v1.1 of Mabec Games' fork of
[davidptrssvea's Windows recompilation](https://github.com/davidptrssvea/Table-Tennis-Rockstar-PC),
focusing on SteamOS / Steam Deck performance and display support while
retaining all v1.0 Vulkan, controller, and AppImage features.

**Pressing Escape immediately quits the game, without a confirmation dialog.**

## What's new in v1.1

- **Kernel-independent user-space page table (`linux_memory_query.cpp`):**
  v1.0 relied on the Linux 6.11+ `PROCMAP_QUERY` ioctl to accelerate
  `rex::memory::QueryProtect`, silently falling back to `/proc/self/maps`
  text scanning on SteamOS 3.5/3.6 (Linux 6.1/6.5) and Ubuntu 24.04 GA
  (Linux 6.8). In addition, `rex::memory::AllocFixed` and `rex::memory::Protect`
  in `librexruntime.so` still scanned `/proc/self/maps` directly on every guest
  heap commit and old-protection query. v1.1 tracks all `rex::memory` guest
  mappings (`MapFileView`, `UnmapFileView`, `AllocFixed`, `DeallocFixed`,
  `Protect`) in a lock-free user-space radix page table (~4 ns lookup, zero
  syscalls) on **all** Linux 6.1+ kernels, with a zero-allocation stack-buffer
  `/proc/self/maps` fallback for untracked mappings on pre-6.11 kernels.
- **Steam Deck `800p (Steam Deck)` (`1280x800`) resolution preset:**
  Added a dedicated `800p (Steam Deck)` preset (and `800p` CLI/config alias)
  in the Linux launcher and profiling tool, running at `1280x800` with `1x`
  internal Xenos EDRAM rendering scale to match the Steam Deck display without
  triggering 2x/3x EDRAM bandwidth multiplication.

## Download and build

- **`Table-Tennis-Linux-v1.1-x86_64.AppImage`** — Linux launcher and native game.
- **`Table-Tennis-Linux-v1.1-x86_64.AppImage.sha256`** — AppImage checksum.
- **`Table-Tennis-Linux-Source-v1.1.zip`** — integration source, tools, tests,
  documentation and presentation images.
- **`SHA256SUMS.txt`** — checksums for the AppImage and source ZIP.

Download the AppImage, make it executable and run it:

```bash
chmod +x Table-Tennis-Linux-v1.1-x86_64.AppImage
./Table-Tennis-Linux-v1.1-x86_64.AppImage
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
PCs, start with **1080p** and **60 FPS**. The v1.1 AppImage and `800p (Steam Deck)`
launcher option have been verified on CachyOS, and the pre-6.11 kernel fallback
path has been verified via `TABLETENNIS_DISABLE_PROCMAP_QUERY=1` integration
tests; physical Steam Deck hardware testing remains pending.

Use tag **`linux-v1.1`** to distinguish this release from upstream tags.
