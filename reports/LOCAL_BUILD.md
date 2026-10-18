# Local build and source distribution

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
