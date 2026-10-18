# Changelog

## v1.0.2 — 2026-10-18

- Added a standalone graphical installer: choose ISO, allow components, Install, then Play. The public installer contains only project-authored installer code and the reviewed source payload.
- Added automatic, checksum-verified portable Python setup. Players do not need to install Python themselves.
- Added automatic setup of missing Microsoft C++/Windows SDK/CMake/Ninja/Clang components through Microsoft's signed installer, after the user's consent and any Windows permission prompt.
- Validate the retail ISO before attempting large compiler installations.
- Added installation status, expandable log, restart guidance, repair/retry and a desktop shortcut.
- Added Visual Studio's bundled Clang discovery and ISO path updates that retain existing graphics settings.
- Added embedded-payload validation so game files or altered source cannot accidentally be bundled into the installer.
- Replaced the player-facing manual build instructions with the simple installer download. Manual setup remains available for developers.

Validation: portable Python downloaded and checksum-verified; full local backend build with existing compilers and supplied SDK passed. GUI rendered and missing-ISO validation checked. First-time Microsoft component installation and the previously blocked ReXGlue download path are not verified on a clean Windows PC.

## v1.0.1 — 2026-10-18

- Changed distribution to local build tools: no retail menu archive, game executable, XEX or generated game functions in the public source package.
- Added an ISO picker and automated extraction, version validation, code generation, patching, CMake build, launcher compilation and runtime staging.
- Added pinned SDK download verification and an offline installed-SDK option. Prerequisite compilers and Python must be installed separately.
- Recreated the verified menu and intro fixes as locally applied patch instructions with input/output hash checks.
- Removed Xbox Live from the local game's main menu and corrected Options/Extras indexes.
- Added immediate intro interruption by gamepad, a loading transition and a large antialiased loading message.
- Removed machine-specific fallback ISO paths from the published integration and launcher source.
- Added malformed-image checks, source-package auditing and tests that use synthetic data rather than game files.

Local-build output remains private and excluded from Git. This change does not itself remove artifacts from old Git history or third-party copies. Local generation is a distribution change, not a legal guarantee.

## Earlier distribution

The earlier v1.0.0 distribution included generated game code/binaries and a modified retail menu archive. Descriptions claiming it contained no game resources were inaccurate. The new workflow does not require downloading that package.
