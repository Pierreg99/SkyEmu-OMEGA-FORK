<sub>[SkyEmu OMEGA](../README.md) › [Docs](README.md) › Handoff</sub>

# Handoff: stand-alone Android APK and README visuals

Technical notes for whoever continues this work, human or agent. The owner-facing summary is in
[HANDOVER.md](HANDOVER.md).

| | |
|---|---|
| Branch | `ccr-e41ac3c8-r3njjn` |
| Base | `89a26e8` *Fix Android JNI lifetimes and support current Windows and Android CI toolchains* |
| Pull request | None opened yet |
| Date | 2026-10-19 |

## State at handoff

| Item | State |
|---|---|
| APK builds locally | ✅ `:apk:assembleRelease`: one APK per ABI (~22 MB each) plus a universal one (~77 MB), `versionCode 2` |
| APK signature, launcher, 16 KB pages | ✅ `apksigner verify`, `aapt2 dump badging` (launchable activity), `tools/verify_android_pages.py` |
| Runs in an emulator | ✅ Android 11 x86_64 emulator (software rendering): launches, menu opens, **Load ROM** opens the system picker, a picked ROM runs, recent games reload, a ROM opened from another app (`content://` VIEW intent) runs, on-screen D-pad / A / B / Start / Select appear on touch |
| Runs on a real phone | ❌ not confirmed. The owner's phone could install the second build but reported *no functions* (fixed below) |
| Regression tests | ✅ `cmake -DSKYEMU_TESTS_ONLY=ON` + `ctest`: 3/3 pass, including new settings migration checks |
| Android CI | ✅ runs 67 and 68 passed on earlier commits; later commits run on push |
| Sound | ⚠️ not checked; the emulator's audio server was busy and its log showed constant `AudioTrack` stop/start, which may only be emulator slowness |
| README rendering on GitHub | ❌ not checked on github.com (callouts, Mermaid, badges) |

## What changed

### Packaging

| File | Change |
|---|---|
| `tools/android_project/apk/build.gradle` | **New** application module. `applicationId com.sky.SkyEmu.omega`, ABI splits plus a universal APK, compressed native libs (`useLegacyPackaging = true`), `ndkVersion 28.2.13676358`, release signed with `signingConfigs.debug`, depends on `project(':app')` and the Kotlin BOM 1.8.22 |
| `tools/android_project/apk/src/main/res/values/strings.xml` | **New.** App name *SkyEmu OMEGA* |
| `tools/android_project/apk/src/main/AndroidManifest.xml` | **New.** App icon and a `MAIN` + `LAUNCHER` filter for `EnhancedNativeActivity`; everything else merges in from the library |
| `tools/android_project/settings.gradle` | `include ':apk'` |
| `.github/workflows/deploy_android.yml` | Builds `:app` and `:apk`, verifies every APK, uploads `AndroidRelease-APK` next to `AndroidRelease-AAR` |
| `tools/verify_android_pages.py` | Accepts APKs, checks zip offsets of uncompressed libraries, `--abis` for single-ABI APKs |

### Runtime fixes ("the app has no functions")

| File | Change |
|---|---|
| `src/sokol/sokol_app.h` | `_sapp_android_touch_event`: `ACTION_DOWN` / `ACTION_UP` produce `TOUCHES_BEGAN` / `TOUCHES_ENDED` again, as in upstream sokol and upstream SkyEmu. Commit `4274165` (*add test logic without touch logic*) had added a `break` after each, so a one-finger tap never reached the GUI: no button worked |
| `src/main.c` `se_draw_emulated_system_screen` | `touch_controller_active` is computed from the last touch time again (upstream). It was hard-coded to `false`, which sized the on-screen gamepad to zero |
| `src/main.c` `se_draw_theme_region_tint_partial` | `gamepad_mask` is cleared only when *Avoid overlapping touchscreen* is off (upstream). It was cleared unconditionally, so the setting did nothing |
| `src/se_settings.{h,c}` | Settings version 6. Defaults and a one-time migration: avoid overlap in portrait (upstream default) and, on phones, the bezel-less layout. The default skin's portrait bezel pins the screen to the bottom with the controls over it, and the design systems draw that bezel transparent anyway. `SE_AVOID_OVERLAP_*` moved here from `main.c` |
| `src/http_control_server.cpp` | On Android and iOS the server listens on `127.0.0.1` instead of `0.0.0.0`. This fork enables it by default for host apps, which made every phone's emulator controllable from the local Wi-Fi with no authentication |
| `tools/se_runtime_test.c` | Checks for the new defaults and the v6 migration |

### Docs

`README.md` (badges, APK button, callouts, nav, design gallery, Mermaid diagram, *Install on Android*),
`docs/BUILDING.md`, `tools/android_project/README.md`, `docs/HTTP_CONTROL_SERVER.md` (phones bind to localhost,
use `adb forward`).

## Why it is built this way

- **Separate `:apk` module, library untouched.** Host apps keep depending on `:app`; `:apk` has no code.
- **Launcher filter in `:apk`.** The library's only `LAUNCHER` category is in a `VIEW` filter (no `MAIN`), so
  without it the app has no icon. Adding it in `:app` would give every host app a second launcher entry.
- **Own application ID.** `com.sky.SkyEmu` is upstream SkyEmu's ID; installing over it with another key fails.
- **`ndkVersion` in `:apk`** so AGP can strip `libSkyEmu.so` (unstripped universal APK: 247 MB).
- **Kotlin BOM** to resolve duplicate `kotlin-stdlib` 1.8 / `-jdk7`/`-jdk8` 1.6 classes.
- **Debug signing.** The committed `skyemu-open-signing-store` has no known password or alias.
- **Touch and layout fixes restore upstream behaviour** instead of adding new options. Host apps that embed
  SkyEmu through `NativeBridge` and their own `SurfaceView` don't receive events from the native input queue, so
  the sokol change doesn't affect them. Hosts that want SkyEmu's own controls hidden can set
  `se_android_set_touch_controls_opacity(0)` or use `HideUI()`.
- **Localhost binding instead of turning the server off.** The fork's host-app integration (`LoadHtml`,
  `/input`, `/ping`, `/external_menu` callbacks) relies on the server; binding to loopback keeps that working.

## Reproduce

```sh
sdkmanager 'platforms;android-36' 'build-tools;36.0.0' 'ndk;28.2.13676358' 'cmake;3.22.1' 'platform-tools'
cd tools/android_project
export ANDROID_HOME=/path/to/sdk
./gradlew :app:assembleRelease :apk:assembleRelease
python3 ../verify_android_pages.py --abis arm64-v8a apk/build/outputs/apk/release/apk-arm64-v8a-release.apk
adb install -r apk/build/outputs/apk/release/apk-arm64-v8a-release.apk

# Settings regression tests (host)
cmake -S ../.. -B /tmp/se-tests -DSKYEMU_TESTS_ONLY=ON -G Ninja && cmake --build /tmp/se-tests && (cd /tmp/se-tests && ctest)
```

Testing without a phone: an x86_64 system image runs with `emulator -accel off -gpu swiftshader_indirect` when
there is no KVM. It needs ~10 minutes to boot and renders at ~7 fps, so taps must be held (`input swipe x y x y
400`), and the system file picker is easier to drive with `input keyevent` (Tab, Down, Enter). The HTTP control
server (`adb forward tcp:18080 tcp:8080`, `/settings`, `/setting?...`) is handy for flipping settings.

## Next steps

1. **Try it on the owner's phone**: open a game, play with the on-screen controls, check sound, save states
   and Material You colours. Existing installs migrate their settings to version 6 on first launch.
2. **Sound**: if it crackles or is silent on a real phone, look at `se_init_audio` / sokol_audio's OpenSL ES
   backend (the emulator log showed a stop/start loop).
3. **Check the README on github.com** and the CI run for the latest commit.
4. **Open a pull request** (none exists yet).
5. Optional: real release signing from GitHub secrets, an AAB for the Play Store, version from git tags, and a
   GitHub Release so *Download APK* links straight to a file.

## Out of scope

`Pierreg99/KiBlox-VoxelGame` shares the branch name but was not changed: it is a browser game with no Android
target, and its README comes from the owner's own template tooling.
