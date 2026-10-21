# Changelog

## Linux v1.1 — 2026-10-21

SteamOS / Steam Deck performance and resolution update for the Mabec Games
Linux fork (`linux-v1.1`).

- **Kernel-independent O(1) guest memory tracking (`linux_memory_query.cpp`):**
  Replaced the Linux 6.11+-only `PROCMAP_QUERY` requirement for guest memory
  with a lock-free user-space radix page table interposing `MapFileView`,
  `UnmapFileView`, `AllocFixed`, `DeallocFixed`, `Protect`, and `QueryProtect`.
  Eliminates `/proc/self/maps` text scanning on SteamOS 3.5/3.6 (Linux 6.1/6.5)
  and Ubuntu 24.04 GA (Linux 6.8) during GPU write-watch `SIGSEGV` faults,
  guest heap commits (`AllocFixed`), and old-protection queries (`Protect`),
  with a zero-allocation stack-buffer `/proc/self/maps` fallback for untracked
  addresses on pre-6.11 kernels.
- **Steam Deck `800p (Steam Deck)` (`1280x800`) resolution preset:**
  Added a dedicated `1280x800` (`1x` internal Xenos EDRAM scale) preset and
  `800p` CLI/config alias to the Linux launcher and profiler.
- **Validation status:**
  User confirmed the v1.1 AppImage and new `800p (Steam Deck)` launcher option
  work on CachyOS, and integration tests verified the pre-6.11 SteamOS memory
  path (`TABLETENNIS_DISABLE_PROCMAP_QUERY=1`). Physical Steam Deck hardware
  validation remains pending.

## Linux v1.0 — 2026-10-20

First Linux release of the Mabec Games fork, based on davidptrssvea's Windows
v1.0.5 source. Linux release tags use `linux-v1.0` to keep platform versions
distinct.

- Added native Linux build support and a Tk settings launcher with supplied
  artwork, saved settings and Vulkan GPU selection.
- Corrected Vulkan texture exponent-bias decoding, restoring character skin.
- Optimized Linux memory-permission queries, retaining a legacy fallback.
- Added local AppImage packaging with the runtime, corrected graphics plugin,
  Python/Tk and Vulkan loader bundled; no player SDK installation is required.
- Confirmed AppImage launcher/game operation on CachyOS and controller rumble
  during Linux gameplay.
- Added optional CPU/graphics diagnostics and Debian/Arch launcher smoke tests.
- Documented that Escape immediately quits the game without confirmation.

The release provides the Linux v1.0 AppImage, checksums and a separate source
archive. Debian/Ubuntu desktop testing and clean-machine validation remain pending.

## Upstream Windows history

## v1.0.5 — 2026-10-19

- Added a transparent Windows desktop icon at 16, 24, 32, 48, 64, 128 and 256 pixels.
- The installer-created desktop shortcut now uses the icon and opens the settings launcher. The launcher executable also embeds the icon.
- Credited icon artist Gary aka POOTERMAN :) with a link to his DeviantArt profile in README and the installed credits.
- Existing installations can run this installer in the same folder to refresh the launcher and desktop shortcut.

## v1.0.4 — 2026-10-18

- Fixed apparent endless `LOADING MAIN MENU...` on first launch: the loading overlay covered the game's missing-save question, while automatic Start input prevented answering it.
- Pause the overlay and automatic input when the retail startup message-dialog movie is active. Real controller input becomes available after releasing the skip button.
- Keep the original choice to create a saved game or continue without saving; do not silently choose for the player or replace existing saves.
- Reapply release-before-input handling when leaving a startup dialog for the main menu, so a held confirmation cannot select Tournament.
- Tested the first-run missing-save screen, new save creation and confirmation, continuing without saving, and automatic startup with the newly created save in isolated local profiles.

Repair: run the new installer with your ISO and the same installation folder. At the first-run **Saved Game** question, choose **A** to create a save or **B** to continue without saving; follow any further confirmation on screen.

## v1.0.3 — 2026-10-18

- Fixed startup incorrectly rejecting an existing ISO with `--game_data_root does not exist`. The stock SDK v0.10.0 accepts only a directory; previous tests used an installed SDK whose application source had already been modified for ISO mounting.
- Include the BSD-licensed ISO-capable application adapter in the reviewed source payload and compile it instead of the SDK's directory-only application source. The installed SDK stays unchanged.
- Mount the user's ISO read-only as `game:` and `d:` before loading the XEX, including disc images named with uppercase `.ISO`.
- Use an explicit `--game_data_root="..."` launch argument and preserve saved ISO paths containing an equals sign.
- Missing-disc errors now explain how to select the ISO again in the launcher.
- Added an opt-in runtime regression test that boots a private build with an ISO outside the install folder, using both command-line selection and saved settings from a different working directory.

Repair: download the v1.0.3 installer, select your ISO and the **same installation folder**, then click Install / Recheck. No deletion or fresh Windows component installation is required when the components are already present.

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
