# Table Tennis for Linux v1.3.1

Linux v1.3.1 of Mabec Games' fork of
[davidptrssvea's Windows recompilation](https://github.com/davidptrssvea/Table-Tennis-Rockstar-PC).

**Pressing Escape immediately quits the game, without a confirmation dialog (or closes active PC Settings / Achievements overlays).**

## What's new in v1.3.1

- **Steam Deck precaution for the v1.3 session crash
  ([issue #4](https://github.com/Mabec-games/Table-Tennis-Rockstar-Linux/issues/4)):**
  v1.3 could take down the Steam Deck session, and ran slower than v1.2 there.
  The responsible change has not been identified yet, so on a Steam Deck
  v1.3.1 automatically returns the most likely candidates to their v1.2
  behaviour. This is a precaution, not a confirmed fix: please report on the
  issue whether v1.3.1 works on your Deck.
- **What a Steam Deck does differently in v1.3.1:** frame pacing runs as in
  v1.2; the picture is no longer stretched to the 16:10 screen during matches;
  the achievement popup, list and sound are off (achievements still unlock and
  are saved); the automatic update check at launch is skipped (**OPTIONS > PC
  SETTINGS > UPDATES** still checks on demand); button icons do not follow the
  controller type.
- **Each of these can be switched individually** with an environment variable
  set to `1` (v1.2 behaviour) or `0` (v1.3 behaviour), on any machine:
  `TT_LEGACY_PACING`, `TT_NO_ASPECT`, `TT_NO_ACHIEVEMENT_UI`,
  `TT_NO_UPDATE_CHECK`, `TT_NO_PROMPTS`. In Steam, use launch options such as
  `TT_NO_ACHIEVEMENT_UI=0 %command%`. Active switches are written to the log.
- **Game window icon:** the game window now shows the application icon on X11
  desktops ([issue #3](https://github.com/Mabec-games/Table-Tennis-Rockstar-Linux/issues/3)).
- **Credits:** the notice and license for code adapted from
  [ThisIsAkill/Table-Tennis-Linux](https://github.com/ThisIsAkill/Table-Tennis-Linux)
  (achievements UI, online and System Link play, relay server, PlayStation
  prompts, PC SETTINGS menu, photo mode) are restored and credited in the README
  ([issue #1](https://github.com/Mabec-games/Table-Tennis-Rockstar-Linux/issues/1)).
- **Source build:** the v1.3 native hooks are now recorded in
  `project/tools/native_edits.json`, so a build from source includes frame
  pacing, ultrawide support and the quit menu.

Behaviour on other PCs is unchanged from v1.3. See the
[v1.3 notes](RELEASE-LINUX-v1.3.md) for the v1.3 feature list.

## Compatibility

The AppImage targets SteamOS 3.5+ (Steam Deck), current Arch, Debian 13+ and
Ubuntu 24.04+, with an x86-64-v3 CPU, Linux 6.1+ and a working Vulkan driver.
v1.3.1 was run on CachyOS only; it has not been tested on a physical Steam
Deck.

Use tag **`linux-v1.3.1`** to distinguish this release from upstream tags.
