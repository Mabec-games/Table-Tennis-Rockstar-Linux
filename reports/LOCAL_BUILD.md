# Local build and source distribution

## v1.0.2 graphical installer

The public `Install-Table-Tennis.exe` is compiled from `project/installer/Installer.cs`
and embeds the 31-file reviewed source ZIP. It contains no compiled game or retail
archive. The installer builder verifies all ZIP member names, uniqueness and bytes
against the public source allowlist before embedding it.

The GUI chooses ISO/destination, obtains download/install consent, extracts only
the source payload and runs `bootstrap.ps1`. The bootstrap downloads the official
Python 3.14.8 x64 embedded runtime and verifies its published SHA-256. Python stays
isolated under `.tools`; no system Python installation or pip packages are needed.

The selected retail ISO is validated before compiler installation. If the compiler
check fails, the bootstrap verifies Microsoft's installer Authenticode signature
and requests the VC tools workload, recommended Windows SDK components, CMake and
Clang. Only that Microsoft installation requests elevation. Restart codes are
reported without restarting automatically. The project does not disable security
controls or bypass blocked downloads.

Sources for component IDs and the pinned runtime:
- https://learn.microsoft.com/en-us/visualstudio/install/workload-component-id-vs-build-tools?view=vs-2022
- https://learn.microsoft.com/en-us/visualstudio/install/use-command-line-parameters-to-install-visual-studio
- https://www.python.org/downloads/release/python-3148/

The bootstrap was run from an unpacked source package, downloaded/verified the
portable Python runtime, validated the ISO, and completed all 95 C++ build steps,
launcher compilation and runtime staging using existing compilers and a supplied
SDK. The GUI rendered correctly and refused to begin without an ISO. Automatic
installation of missing Microsoft components was not exercised on this PC.
The first-time ReXGlue download remains unverified due to the earlier security block.

All nine automated tests passed. These include rejection of extra game files in
the embedded payload and verification that ISO-only validation does not inspect
the compiler environment or obtain an SDK. Re-running the final bootstrap with
downloads disabled reused the isolated runtime, applied zero native edits and
completed successfully without changing system components.

The existing working game installation is separate from installer validation.

The public files are integration code, locally applied changes, build scripts,
metadata and third-party notices. Generated game functions and original/modified
retail archives are not part of the source allowlist.

## Pipeline

1. Check Windows x64, Python and the compiler tools before producing game output.
2. Use a supplied ReXGlue SDK v0.10.0, or download the pinned official Windows
   archive and verify SHA-256 before extraction. Never disable security controls.
3. Open the user's ISO read-only. Parse the bounded GDFX directory tree, reject
   cycles/out-of-image entries, and verify the supported executable's SHA-256.
4. Extract only default.xex and assets/resource/flash/flash.rpf into ignored local
   folders. Check the original archive/movie hashes before applying menu changes.
5. Generate C++ from the user's XEX with the supported SDK and function metadata.
6. Apply 17 verified native edit rules (16 functions plus the SDK-derived PCH).
   Rules contain hashes, positions and replacement text, not original bodies.
   Each rule recognizes both supported clean and patched output.
7. Configure and compile the local game. Compile the project-authored launcher,
   copy SDK runtime dependencies and license notices, and create local settings.

The patch rules reproduce the preceding working integration, including offline
profile hooks, high-refresh pacing, intro stepping and input filtering. Segoe UI
Semibold is loaded from Windows for the 39px loading text, with fallback fonts.
The two adapted SDK headers preserve the x64 FP exception-mask correction without
editing the user's installed SDK.

## Verification

- Extracted the supported XEX and menu archive from the user's ISO locally.
- The patched menu archive SHA-256 matches the preceding verified game build:
  BF0A4D39A784D9924FB03FE7B7B21380F5AC4B337932F6946B52F9D23CABD6F9.
- Regenerated game code from the retail XEX; all 17 native rules applied and
  a repeat application changed zero files.
- Tests cover read-only ISO access, bounds/cycle rejection, unsupported input,
  native hash validation/idempotence and public package contents.
- The automatic SDK download was blocked by Windows security on this PC. It
  remains unverified end to end; the build test uses the installed SDK option.
- A full clean local build passed: retail ISO extraction, code generation,
  all native/menu patches, 95 C++ build steps, linking and C# launcher compilation.
- All 16,971 generated function bodies match the preceding working integration
  after applying the local rules. The new build reached the normal title screen;
  its runtime log confirms early gamepad initialization, the local menu mount and
  loading-font registration. No new physical-controller test was requested.
- All seven synthetic-input tests passed. The public Git index and the 28-file
  source package passed the explicit allowlist audit.

Public source packaging and a fresh local compilation are checked separately.
The ISO, generated source and resulting executable must not be added to Git or
the release ZIP. Old Git objects/release assets require separate cleanup; an
ignore rule or a new commit does not erase them.
