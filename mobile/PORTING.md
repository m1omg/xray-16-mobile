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
5. **iOS**: same CMake tree, SDL2 UIKit backend, LuaJIT interpreter-only (no JIT on iOS),
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

- 2026-10-30: repo pushed to github.com/m1omg/xray-16-mobile (branch `mobile`); public since
  2026-11-02 so macOS CI minutes are free. Upstream's CI workflows are disabled in the repo settings.
- 2026-11-02: **iOS** build scripts + `ios.yml` workflow (manual dispatch, macos-15). Deps build
  first time; engine needed: app-bundle target, UIKit framebuffer for Present(), `SDL_OpenURL`
  instead of `system()`, LuaJIT probe targeting iOS (interpreter only).
- 2026-11-02: **touch controls** (`xrEngine/xr_input_touch.*`), tested on the emulator: tap through
  intros (tap = Escape when there is no cursor), tap-to-point menus, new game, skip cutscene,
  floating move stick, swipe look, on-screen buttons, inventory open/close, Android back = Escape.
  `release/xray-soc-0.2-touch-arm64.apk` (local) for the S24 / Pad 5.
- 2026-11-02: first iOS IPA from CI (run 3: arm64, iOS 15+, system frameworks only, 19.7 MB).
- 2026-11-02: fixed a fatal assert a few minutes into Cordon (dogs playing CoP-only idles); survives a
  10-minute soak now. The stale `GL_INVALID_VALUE` was glDeleteShader(0) for "null" stages
  (found with the new `-gl_debug`); none left. Android launcher is an import screen (system folder picker → copies
  `gamedata.db*` with progress, resumable), tested end to end on the emulator.

## How to run (current)

Android:
1. `scripts/fetch-deps.sh`, `scripts/build-deps-android.sh <abi>`, `scripts/build-engine-android.sh <abi>`,
   `scripts/package-android.sh arm64-v8a [x86_64]` → `android/app/build/outputs/apk/debug/app-debug.apk`
2. Install and launch. The first launch shows an import screen: pick the folder holding the game's
   `gamedata.db0`…`gamedata.dbd` (phone storage, SD card, USB drive via the system picker) and the
   app copies them into `Android/data/org.xraymobile.soc/files/` (resumable). Or push them with
   `scripts/push-gamedata.sh` over adb.
4. `EXTRA_ARGS="-start server(all/single/alife/new) client(localhost)" scripts/run-device.sh 120`
   starts a new game directly.

iOS (macOS only, or the `iOS` workflow on GitHub Actions → artifact `X-Ray-SoC-ios-<n>`):
1. `scripts/fetch-deps.sh && scripts/build-deps-ios.sh && scripts/build-engine-ios.sh && scripts/package-ios.sh`
2. Sideload the unsigned `X-Ray-SoC.ipa` (AltStore/SideStore/Sideloadly with an Apple ID).
3. Copy `gamedata.db*` into the app's folder in Files (On My iPhone → X-Ray SoC).
   `Documents/commandline.txt` (one line) adds engine arguments.

Touch settings (console / `user.ltx`): `touch_controls 0|1`, `touch_look_sens`, `touch_opacity`.

## Known issues / next steps

1. **Unverified: empty inventory at the start.** In Sidorovich's bunker the backpack is empty (4.3 kg
   counted) and slots 1/5 draw nothing ("Set slots blocked" in the log). Could be SoC's scripted start
   (only knife/binoculars/bolts/PDA, all in persistent slots SoC's UI doesn't show; first pistol from
   Wolf) or a bug — check outside the bunker. (`slots_count = 11, but real slots count is 10` is
   harmless: SoC's system.ltx declares 11 slots and defines 10.)
2. **4:3 UI stretched** to 20:9. Needs pillarboxed UI or SoC widescreen UI handling.
3. **Texture memory**: DXT→RGBA8 fallback is active on the emulator; bunker = 392 MB. Whether the
   S24 (Xclipse 940) and Pad 5 (Adreno 640) sample DXT natively shows in the log
   ("GLES DXT textures: native / decoded to RGBA8 on load"); iOS never does.
   Plan: offline DXT→ASTC transcode into loose `gamedata/textures`.
4. **SoC compat** (OpenXRay #392): CoP dog animations the SoC model lacks (`stand_drage_` etc.; the
   fatal one is fixed, the rest only log), missing intro sounds
   `characters_voice\scenario\video\*`, tutorial `game_loaded` missing, "Version conflict in shader".
6. Real-device testing (S24, Pad 5): performance, GLES 3.2 base-vertex path, S3TC availability,
   lifecycle (pause/resume, context loss), touch layout on a 16:10 tablet.
6. Touch polish: pinch-zoom / two-finger scroll for the PDA map and long lists, button layout editor,
   haptics, hiding controls during cutscenes.
7. iOS: first device run (nobody has tested an IPA yet), GLES deprecation → ANGLE-Metal fallback.
