# Changelog

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
