# S.T.A.L.K.E.R.: Shadow of Chernobyl → Android / iOS

Workspace for porting SoC (GOG 1.0006, built 2008-03) to mobile.
Game install (data source + RE target): `../S.T.A.L.K.E.R. Shadow of Chernobyl/`

## Strategy

**Engine:** OpenXRay (`xray-16/`, upstream `c62d9d3`, 2026-10-15) instead of a from-scratch decomp.

- The SoC binaries are ~6 MB of MSVC x86 code (`xrGame.dll` alone: 4.4 MB `.text`, 2070 RTTI classes).
  A full decomp is a months-long swarm job and would regenerate GSC's code wholesale.
- OpenXRay already runs on Linux/ARM64 and has a **GLES 3.0 (WebGL2) renderer path** behind
  `XR_PLATFORM_WEB`: ESSL shader rewriter (`rgl_essl_web.h`), gli `PROFILE_ES30`, `gladLoadGLES2`.
  That path is the basis for both Android (native GLES 3.x) and iOS (GLES 3.0 / ANGLE-Metal).
- OpenXRay does **not** fully support SoC yet (issue #392, active as of 2026-07). A
  `ShadowOfChernobylMode` / `USE_SHOC_RESOURCES` switch exists. Closing the SoC gaps is the
  main work. The user's SoC binaries (Ghidra/REA) are the behavioral reference for that.

## Phases

1. **Toolchain**: SDK cmdline-tools, NDK, CMake 3.31, JDK 21 (Gradle), JDK 25 (Ghidra 12.2). → `toolchain/`
2. **Android build**: NDK cross-compile of the engine plus deps (SDL2, OpenAL Soft, ogg/vorbis/theora,
   lzo, libjpeg-turbo, LuaJIT). Introduce `XR_GLES` (WEB|ANDROID|IOS) where `XR_PLATFORM_WEB` really
   means "GLES context". SDLActivity-based APK. ABIs: arm64-v8a (phones), x86_64 (emulator/KVM).
3. **Boot to main menu** on the emulator using the user's gamedata (copied to app external storage).
4. **SoC compatibility** (#392 checklist): iterate on a Linux desktop build (fast), use Ghidra on
   SoC `xrGame.dll` for behavior. Script-API diff: `re/soc/xrGame_ident_strings.txt` vs OpenXRay exports.
5. **Mobile UX**: touch controls overlay, UI scale, DXT texture fallback (CPU decode / ETC2-ASTC
   transcode where `GL_EXT_texture_compression_s3tc` is missing, e.g. Mali), perf (R1-class
   settings), lifecycle (pause/resume, GL context loss).
6. **iOS**: same CMake tree, SDL2 UIKit backend, LuaJIT interpreter-only (no JIT on iOS),
   GLES 3.0 via EAGL first, ANGLE-Metal later. **Requires macOS + Xcode** (or a GitHub Actions macOS
   runner) to build, and an Apple ID to sign/sideload (AltStore/SideStore, or EU alt marketplace).

## Known constraints

- No game data is ever bundled in the APK/IPA; the user supplies their own copy.
- REA's Ghidra provider needs `GHIDRA_INSTALL_DIR` in the MCP server environment (and is verified
  against 12.1.x; local install is 12.2, which needs Java 25).

## Status log

- 2026-10-30: workspace created, OpenXRay cloned, SoC RTTI/identifier strings extracted to `re/soc/`.
- 2026-10-30: toolchain installed (NDK 30.0.16248370, CMake 3.31.6, build-tools 36.1, android-36,
  emulator + android-35 x86_64 image, JDK 21/25). Ghidra project `re/ghidra/soc` has xrGame.dll
  analyzed: **26,313 functions**, 4.2 MB code, 245 s auto-analysis.
- 2026-10-30: engine branch `mobile` (uncommitted):
  - `Platform.hpp`: `XR_PLATFORM_IOS`, and `XR_GLES` (= WEB | ANDROID | IOS).
  - All GLES-API `XR_PLATFORM_WEB` guards (31 files) → `XR_GLES`; Emscripten-only bits in `glHW.*` stay WEB.
  - `glHW.cpp`: native ES context via SDL (`SDL_GL_CONTEXT_PROFILE_ES` 3.0) + `gladLoadGLES2`;
    `glDrawElementsBaseVertex` for base-vertex draws instead of the WebGL extension.
  - `xr_3da`: built as `libmain.so` on Android; includes `SDL_main.h` on Android/iOS.
  - None of this has been compiled yet.
- 2026-10-30: user approved builds. Deps + engine build for arm64-v8a and x86_64 with 0 errors.
  First APK, then on the x86_64 emulator (Android 15, host GPU, GLES 3.1):
  boots → mounts all 14 encrypted SoC archives → main menu with animated background →
  `-start server(all/single/alife/new) client(localhost)` loads Cordon → intro video →
  **in-game in Sidorovich's bunker, deferred lighting/shadows, HUD, A-Life running.**
  Screenshots in `milestones/` (local only).
  Fixed on the way (engine branch `mobile`, 3 commits): see `git log` in `xray-16/`.

## How to run (current)

1. `scripts/build-deps-android.sh <abi>`, `scripts/build-engine-android.sh <abi>`,
   `scripts/package-android.sh arm64-v8a x86_64` → `android/app/build/outputs/apk/debug/app-debug.apk`
2. Install, launch once (creates `Android/data/org.xraymobile.soc/files/`, unpacks shaders + fsgame.ltx).
3. Copy the game's `gamedata.db0`…`gamedata.dbd` into that folder (USB/MTP on a phone,
   `scripts/push-gamedata.sh` for adb).
4. `EXTRA_ARGS="-start server(all/single/alife/new) client(localhost)" scripts/run-device.sh 120`
   starts a new game directly (no touch controls yet).

## Known issues / next steps

1. **Touch controls** — none yet. Engine input is relative-mouse only, so taps don't move the
   UI cursor. Needed: tap-to-point for menus/inventory/PDA, virtual sticks + buttons in-game.
   A Bluetooth/USB gamepad or mouse+keyboard should work through SDL meanwhile (untested).
2. **4:3 UI stretched** to 20:9. Needs pillarboxed UI or SoC widescreen UI handling.
3. **Texture memory**: DXT→RGBA8 fallback is active on the emulator (and will be on Mali);
   bunker = 392 MB of textures. Plan: offline DXT→ASTC transcode into loose `gamedata/textures`.
4. **SoC compat** (OpenXRay #392): dog animation names (`stand_drage_` etc.), inventory
   `slots_count` 11 vs 10, missing intro sounds `characters_voice\scenario\video\*`,
   tutorial `game_loaded` missing, "Version conflict in shader" warnings (shaders.xr).
5. Stale `GL_INVALID_VALUE` attributed to the first texture (`intro\intro_back.dds`, 4×4) —
   real source is an earlier GL call; find with KHR_debug.
6. Real-device testing (Samsung phone, Xiaomi tablet): performance, GLES 3.2 base-vertex path,
   S3TC availability, lifecycle (pause/resume, context loss).
7. Data import UX: folder picker (SAF) instead of manual copying.
8. **iOS** via GitHub Actions (macOS runner): CMake iOS toolchain for deps + engine,
   LuaJIT `LUAJIT_DISABLE_JIT`, static or embedded frameworks, SDL UIKit app; needs a GitHub repo
   for the engine fork + this workspace (ask the user before creating/pushing).
