# Rockstar Games presents Table Tennis — local Windows build tools

This project builds a Windows x64 recompilation locally from **your own retail Xbox 360 disc image** using ReXGlue SDK v0.10.0. The public source package contains integration code, patch instructions and build tools. **It does not contain a game executable, XEX, menu archive, ISO or generated translation of the game's functions.**

## What the local build provides

- An offline main menu with Tournament, Exhibition, Training Mode, Options and Extras. Xbox Live is removed to prevent entering the unavailable online screen.
- A gamepad button immediately stops the startup logos/intro and continues to the main menu after loading, without another Start press. The skip press cannot accidentally select a menu item.
- A large, centered, antialiased **LOADING MAIN MENU...** message while the menu initializes. Windows supplies the font.
- The graphical launcher, display and framerate settings, F10 FPS overlay and Escape to exit from the existing integration.

## Requirements

- Windows 10/11 **x64**, a DirectX 12 GPU, and an AVX2 CPU.
- Your own unmodified retail **USA/Europe Xbox 360 ISO**, Title ID `545407DF`. Other game executable versions are rejected before generating files.
- [64-bit Python 3.10 or newer](https://www.python.org/downloads/windows/), available as `py` or `python`.
- [Visual Studio 2022 Build Tools](https://visualstudio.microsoft.com/downloads/) with **Desktop development with C++**, MSVC x64 tools, Windows SDK, and **C++ CMake tools for Windows** (CMake and Ninja).
- [LLVM Clang 19 or newer](https://github.com/llvm/llvm-project/releases). The setup finds it on PATH or in `C:\Program Files\LLVM\bin`.
- Several GB of free disk space and time for the first compilation. Build tools require substantially more disk space than the finished local game.

## Install and build

1. Download **Table-Tennis-Local-Build-v1.0.1.zip** from [Releases](https://github.com/davidptrssvea/Table-Tennis-Rockstar-PC/releases) and extract it to a writable folder. Keep the whole folder structure.
2. Install the prerequisites above. The setup does not silently install Python, Visual Studio or LLVM.
3. Double-click **setup.cmd** and select your ISO. Setup downloads the pinned official ReXGlue SDK (about 45 MB), checks its SHA-256, extracts the two required files from your ISO, generates and patches the code, builds the game and launcher, and copies the runtime dependencies locally.
4. When setup reports success, open **dist\tabletennis_launcher.exe** and select your display settings.

The first build is not an instant download-and-play installation. No administrator privileges are needed by the project setup itself; prerequisite installers may request them. The ISO is opened read-only and is never modified. Keep `dist\assets` next to the executable. Do not upload the resulting `dist`, `private_work` or generated code folders.

To use an existing SDK without any download:

```powershell
.\setup.ps1 -IsoPath 'E:\Games\TableTennis.iso' -SdkPath 'E:\Tools\rexglue-sdk-0.10.0' -NoDownload
```

`SdkPath` must contain `bin\rexglue.exe` and `lib\cmake\rexglue`. SDKs downloaded automatically are cached under `.tools` for subsequent builds. If Windows security blocks the SDK, setup stops; it does not alter security settings. Download/bootstrap could not be verified end to end on the development PC because its security software blocked the official archive. The installed-SDK path was used for the local build test.

For lower memory use, add `-Jobs 2`. For extraction, patching and code generation without compilation, add `-PrepareOnly`.

## Upgrading an existing local installation

Keep your existing `user_data` and ISO. Extract this source release to a new folder and build there. The new launcher writes settings only on your computer. The menu override is recreated from your ISO; you do not need to obtain it from an older release. Save/load was not revalidated for this update.

## Controls

| Input | Action |
|---|---|
| Gamepad button during startup | Stop the intro and continue to the main menu after loading |
| Gamepad | Original game controls |
| F10 | Toggle the FPS overlay |
| Escape | Exit to Windows |

## Developer checks and packaging

From the repository root:

```powershell
py -3 -m unittest discover -s tests -v
py -3 project/tools/package_source.py --audit-index
py -3 project/tools/package_source.py --output 'E:\ReleaseOutput'
```

The package command uses an explicit allowlist. It never packages local game files, SDK binaries, user settings, saves or logs. The index audit also rejects tracked files outside that allowlist. `.gitignore` excludes original-derived output; it does not remove files from old Git history.

Code generation runs before CMake configuration. `native_edits.json` records positions, hashes and replacement text for project changes; it omits the removed original code. The patcher validates every affected function and is idempotent. Do not regenerate and compile without rerunning the patcher; use setup for each build. See [implementation and verification](reports/LOCAL_BUILD.md) and the [changelog](CHANGELOG.md).

## License and rights

Project-authored integration and tools use the [MIT license](LICENSE). Two adapted ReXGlue SDK headers and SDK-derived CMake integration retain their BSD 3-Clause notices; see [ReXGlue license](licenses/ReXGlue/LICENSE.txt). SDL's notice is retained under [licenses/SDL3](licenses/SDL3/LICENSE.txt). The SDK and its dependencies are obtained separately.

Rockstar Games and Take-Two retain rights to their game and original material. This project is independent and unaffiliated. No project license grants rights to the original game. Creating original-derived output locally reduces what this project distributes; **it is not a guarantee of legal compliance**. You are responsible for having the necessary rights to use and process your copy.
