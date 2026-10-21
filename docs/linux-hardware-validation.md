# Linux hardware validation

The local laptop is a Ryzen 9 6900HS system with an NVIDIA GeForce RTX 3080
Laptop GPU and an AMD Radeon 680M APU GPU.

On 2026-10-20, the user confirmed that
`dist/Table-Tennis-Linux-x86_64.AppImage` launched its launcher and game correctly,
with no issues, on this CachyOS (Arch-based) laptop. This confirmation does not
specify the selected GPU/backend or establish a measured frame rate.
Clean-machine and Debian/Ubuntu AppImage desktop tests remain pending.

| Path | Result | Evidence |
| --- | --- | --- |
| NVIDIA RTX 3080, X11 | Selected by Vulkan auto-enumeration; swapchain created | `build/linux/logs/tabletennis_034.log` |
| AMD Radeon 680M (RADV), X11/XWayland | Black window; swapchain creation failed | `build/linux/logs/tabletennis_035.log` |
| AMD Radeon 680M (RADV), native Wayland (1080p) | User confirmed correct visuals and gameplay; swapchain created and runtime stayed near the 60 FPS cap | `build/linux/logs/tabletennis_036.log`, `graphics-cpu-35518-35528.report.txt`, `guest-cpu-35518-35562.report.txt` |
| AMD Radeon 680M (RADV), native Wayland (800p, `taskset -c 0-3` = 2 cores / 4 threads) | v1.2 match fixes improved 2-character rally cadence from ~45.9–54.0 swaps/s (`156526`) to ~49.4–56.3 swaps/s (`157060`) under a 2-core constraint, cutting GPU worker `memcpy` from 17.85% to 9.63% and `WAIT_REG_MEM` stalls by ~100x vs v1.1 | `graphics-cpu-157060-157070.report.txt`, `guest-cpu-157060-157107.report.txt` |

The Wayland result validates this hybrid laptop and software stack. It does not
show that AMD generally requires Wayland, or that every AMD GPU works on every
desktop backend. The X11 failure and Wayland success indicate the display path
matters on this machine.

The launcher GPU menu uses `vulkaninfo --summary` when available, then passes the
selected Vulkan index as `--vulkan_device`. The saved device is identified by
name and resolved against current enumeration at launch. Auto is the default and
prefers a device Vulkan identifies as discrete, falling back to the loader's
device order. GPU indexes can change when drivers or environment variables change.

GitHub Actions smoke-tests the launcher and its Tkinter dependency in Debian 13
and Arch Linux containers. Those tests cover packaging assumptions, syntax,
settings construction, and dry-run launch arguments; they do not emulate graphics
hardware or replace distro-specific desktop gameplay tests.
