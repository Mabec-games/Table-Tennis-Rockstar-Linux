# Rockstar Games presents Table Tennis — Windows installer

Build and play from **your own Xbox 360 disc image**, with a graphical installer that prepares the required components for you.

## Get started

1. Download **[Install-Table-Tennis.exe — installer preview](https://github.com/davidptrssvea/Table-Tennis-Rockstar-PC/releases/download/v1.0.2/Install-Table-Tennis.exe)**.
2. Open it, choose your retail **Table Tennis ISO**, allow the required components, and click **Install**.
3. When it says **Ready to play**, click **Play**. A desktop shortcut is also created when Windows permits it.

**You do not need to install Python, compilers or SDKs manually. You do not need a command prompt or the source ZIP.**

First setup needs internet access, several GB of free space and time to download components and build the game. If Microsoft build components are missing, Windows may ask for administrator permission. Nothing restarts Windows automatically; if a restart is required, the installer tells you to reopen it afterward.

## What you need

- Windows 10/11 **x64**, an AVX2 CPU and a DirectX 12 GPU.
- Your own unmodified **USA/Europe retail Xbox 360 Table Tennis ISO**, Title ID `545407DF`.
- A writable installation folder. The default is `%LOCALAPPDATA%\TableTennisPC`.

The ISO is checked before installing large compiler components and is always opened read-only. Other game versions are rejected. Keep your ISO available after installation: the game still reads it during play.

## Included game fixes

- Xbox Live removed from the main menu to avoid the unavailable online screen. Tournament, Exhibition, Training Mode, Options and Extras remain available.
- A gamepad button immediately stops the startup intro and continues to the main menu after loading, without another Start press.
- A large, centered, antialiased **LOADING MAIN MENU...** transition while the menu initializes. The skip press cannot accidentally select a menu item.
- The graphical game launcher, display/framerate settings, **F10** FPS overlay and **Escape** to exit.

## If installation stops

Click **Open log** in the installer. You can retry after fixing the reported issue; downloaded components are cached. If Windows security blocks a required component, installation stops and security settings remain unchanged.

The installer is not code-signed. Automatic installation of missing Microsoft components has not yet been tested on a completely clean Windows machine. The installer backend was tested with its downloaded portable Python runtime and existing compiler/SDK installations. The separate ReXGlue download was previously blocked by security software on the development PC; that first-download path remains unverified.

Existing local saves and graphics settings are retained during repair; the selected ISO path is updated. Save/load itself was not revalidated for this update.

## How it works

The small installer contains project source and patch instructions. It downloads a checksum-verified, isolated Python runtime and checks for suitable build tools. If needed, it starts Microsoft's signed Visual Studio 2022 Build Tools installer with the required C++/Windows SDK/CMake/Ninja/Clang components selected. The screen asks permission for component downloads and installation before starting.

It then obtains ReXGlue SDK v0.10.0, checks its pinned SHA-256, reads the supported executable and menu archive from your ISO, generates and patches the game code, and builds everything **on your PC**. No retail menu archive, XEX, ISO, generated game functions or prebuilt game executable is included in the public downloads. `Install-Table-Tennis.exe` is our own installer, not the game executable.

Locally generated files stay under the installation folder. Do not upload `dist`, `private_work` or `project/generated`.

<details>
<summary>Developer / advanced installation</summary>

The **Table-Tennis-Local-Build-v1.0.2.zip** asset is the source package for developers. It is not required by players using the installer.

For manual builds, install 64-bit Python 3.10+, Visual Studio 2022 C++ Build Tools with Windows SDK/CMake/Ninja, and Clang 19+. Clang can be standalone or included in Visual Studio. Then use:

```powershell
.\setup.ps1 -IsoPath 'E:\Games\TableTennis.iso' -SdkPath 'E:\Tools\rexglue-sdk-0.10.0' -NoDownload
```

`SdkPath` must contain `bin\rexglue.exe` and `lib\cmake\rexglue`. Add `-Jobs 2` for lower memory use or `-PrepareOnly` to stop after code generation and patches. The installer can also use a supplied SDK via its optional `--sdk PATH` argument.

Checks and source packaging:

```powershell
py -3 -m unittest discover -s tests -v
py -3 project/tools/package_source.py --audit-index
py -3 project/tools/package_source.py --output 'E:\ReleaseOutput'
powershell -File project/tools/build_installer.ps1 -SourceZip 'E:\ReleaseOutput\Table-Tennis-Local-Build-v1.0.2.zip' -OutputPath 'E:\ReleaseOutput\Install-Table-Tennis.exe'
```

The installer builder validates every embedded source file against the public allowlist before compilation. It rejects extra or altered payload files. Native patch rules store offsets, hashes and replacement text rather than removed original function bodies. See [implementation and validation](reports/LOCAL_BUILD.md) and [changelog](CHANGELOG.md).

</details>

## License and rights

Project-authored integration, installer and tools use the [MIT license](LICENSE). Adapted ReXGlue SDK headers and SDK-derived CMake integration retain their BSD 3-Clause notices; see [ReXGlue license](licenses/ReXGlue/LICENSE.txt). [SDL's notice](licenses/SDL3/LICENSE.txt) is retained. Downloaded components have their own licenses: [Microsoft](https://visualstudio.microsoft.com/license-terms/) and [Python](https://docs.python.org/3/license.html).

Rockstar Games and Take-Two retain rights to their original game and material. This project is independent and unaffiliated. No project license grants rights to the original game. Local generation reduces what this project distributes; it is **not a guarantee of legal compliance**. You must have the necessary rights to use and process your copy.
