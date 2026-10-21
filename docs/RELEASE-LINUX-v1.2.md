# Table Tennis for Linux v1.2

Linux v1.2 of Mabec Games' fork of
[davidptrssvea's Windows recompilation](https://github.com/davidptrssvea/Table-Tennis-Rockstar-PC).

> **Note:** Linux v1.2 supersedes the v1.1 AppImage released earlier today.
> Community testing of v1.1 on a Steam Deck showed that while Training Mode ran
> smoothly, 2-character matches against an opponent still experienced slowdowns.
> The v1.1 AppImage has been removed and replaced by **v1.2**, which resolves
> the 2-character match command-buffer, shared-memory invalidation, and fence
> spin bottlenecks while keeping all v1.0 and v1.1 features.

**Pressing Escape immediately quits the game, without a confirmation dialog.**

## What's new in v1.2 (Steam Deck / RDNA 2 Match Performance Fix)

- **Eliminated `WAIT_REG_MEM` mid-frame sleep stalls (`build_gpu_diagnostics.py`):**
  During 2-character matches, the guest D3D9 command ring splits frames into
  multiple segments synchronized via `WAIT_REG_MEM` packets with `wait >= 0x100`.
  When a segment check missed by even a few microseconds, the GPU worker thread
  previously called `Sleep(wait / 0x100 ms)` (1–2+ ms plus Linux timer slack),
  stalling the GPU command processor for 175–550 ms per second during rallies.
  The production Xenos plugin now uses a tiered spin/yield (`pause` ->
  `MaybeYield` up to ~1.5 ms -> `50 µs` sleep fallback), reducing mid-frame
  `WAIT_REG_MEM` stalls by ~100x.
- **Replaced guest D3D9 command-buffer bus-lock spin (`main.cpp`):**
  Overrode `sub_82359DC0` (the guest D3D9 ring-buffer segment wait loop) so the
  main guest thread uses `_mm_pause()` and `std::this_thread::yield()` instead
  of executing 100-iteration dummy stack load/store loops (`db16cyc` emulation)
  that contended with the GPU worker and RADV driver threads on shared L2/L3
  cache and SMT execution resources.
- **Disabled 256 KB shared-memory invalidation over-widening (`build_gpu_diagnostics.py`):**
  Disabled the 64-page (256 KB) invalidation expansion in
  `SharedMemory::MemoryInvalidationCallback` that caused dynamic skinning writes
  during 2-character matches to invalidate adjacent static geometry and texture
  pages, cutting GPU worker `VulkanSharedMemory::UploadRanges` (`memcpy`) CPU
  time nearly in half on RADV.
- **Eliminated redundant `mprotect` syscalls (`linux_memory_query.cpp`):**
  Added `FindTrackedAccessDiffRange` to `Protect` and `AllocFixed` using the
  user-space radix page table so no-op `mprotect` calls are skipped in user
  space and multi-page protection updates trim unchanged prefix/suffix pages.
- **Includes all v1.1 updates:**
  Kernel-independent O(1) user-space radix page table for Linux 6.1+ (including
  SteamOS 3.5/3.6 on Steam Deck and Ubuntu 24.04 GA) and the dedicated
  **`800p (Steam Deck)` (`1280x800`)** resolution preset.

## Download and build

- **`Table-Tennis-Linux-v1.2-x86_64.AppImage`** — Linux launcher and native game.
- **`Table-Tennis-Linux-v1.2-x86_64.AppImage.sha256`** — AppImage checksum.
- **`Table-Tennis-Linux-Source-v1.2.zip`** — integration source, tools, tests,
  documentation and presentation images.
- **`SHA256SUMS.txt`** — checksums for the AppImage and source ZIP.

Download the AppImage, make it executable and run it:

```bash
chmod +x Table-Tennis-Linux-v1.2-x86_64.AppImage
./Table-Tennis-Linux-v1.2-x86_64.AppImage
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
RTX 3080 and AMD Radeon 680M (RADV, RDNA 2) including core-constrained
(`taskset -c 0-3`) 800p match profiling; follow-up confirmation on physical
Steam Deck hardware is welcome.

Use tag **`linux-v1.2`** to distinguish this release from upstream tags.
