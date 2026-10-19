<sub>[SkyEmu OMEGA](../README.md) › [Docs](README.md) › Handoff</sub>

# Handoff: stand-alone Android APK and README visuals

Technical notes for whoever continues this work, human or agent. The owner-facing summary is in
[HANDOVER.md](HANDOVER.md).

| | |
|---|---|
| Branch | `ccr-e41ac3c8-r3njjn` |
| Commit | `5758736` *Android: build an installable APK and refine README visuals* |
| Base | `89a26e8` *Fix Android JNI lifetimes and support current Windows and Android CI toolchains* |
| Pull request | None opened yet |
| Date | 2026-10-19 |

## State at handoff

| Item | State |
|---|---|
| APK builds locally | ✅ `:apk:assembleRelease` succeeds, 106 MB, 4 ABIs |
| APK signature | ✅ `apksigner verify` passes (Android debug key) |
| Launcher entry | ✅ `aapt2 dump badging` shows `launchable-activity: com.sky.SkyEmu.EnhancedNativeActivity` |
| 16 KB pages | ✅ `tools/verify_android_pages.py` passes on the APK and the AAR; `zipalign -c -P 16` passes |
| Library (`:app`) build | ✅ unchanged, still produces `app-release.aar` |
| Android CI with the new steps | ⏳ first run was in progress at handoff ([Build Android](https://github.com/Pierreg99/SkyEmu-OMEGA-FORK/actions/workflows/deploy_android.yml)) |
| APK runs on a device | ❌ not tested: no device or emulator was used |
| README rendering on GitHub | ❌ not checked on github.com (callouts, Mermaid, badges) |

## What changed

| File | Change |
|---|---|
| `tools/android_project/apk/build.gradle` | **New** application module. `applicationId com.sky.SkyEmu`, namespace `com.sky.SkyEmu.standalone`, `versionCode 1`, `versionName 6.1-omega`, release signed with `signingConfigs.debug`, `ndkVersion 28.2.13676358`, depends on `project(':app')` and the Kotlin BOM 1.8.22 |
| `tools/android_project/apk/src/main/AndroidManifest.xml` | **New.** App icon / round icon / label and a `MAIN` + `LAUNCHER` intent filter for `EnhancedNativeActivity`. Everything else merges in from the library manifest |
| `tools/android_project/settings.gradle` | `include ':apk'` |
| `.github/workflows/deploy_android.yml` | Builds `:app` and `:apk`, verifies both, uploads `AndroidRelease-APK` next to `AndroidRelease-AAR` |
| `tools/verify_android_pages.py` | Accepts APKs (`lib/<abi>/*.so`) and checks that uncompressed libraries start on a 16 KB boundary inside the zip |
| `README.md` | Flat-square badges, *Download APK* / *All builds* buttons, Sol6.1 note as a `[!TIP]` callout, one-line section nav, collapsible light/dark design gallery, Mermaid embedding diagram, *Install on Android* section |
| `docs/BUILDING.md`, `tools/android_project/README.md` | Document the `apk` module and its output path |

## Why it is built this way

- **Separate module, library untouched.** `:app` stays a `com.android.library` so host apps keep embedding it;
  `:apk` is a thin wrapper with no code of its own.
- **Launcher filter in the APK manifest.** The library's only `LAUNCHER` category sits in a `VIEW` filter for the
  `skyemu://oauth` link, with no `MAIN` action, so without the added filter the app installs with no icon.
  It is added in `:apk`, not `:app`, so host apps don't inherit a second launcher entry.
- **`ndkVersion` in `:apk`.** Without it AGP looks for its own default NDK, logs *Unable to strip …
  libSkyEmu.so* and ships unstripped libraries (247 MB instead of 106 MB).
- **Kotlin BOM.** AndroidX brings `kotlin-stdlib` 1.8.22 next to `kotlin-stdlib-jdk7/jdk8` 1.6.21 and
  `checkReleaseDuplicateClasses` fails. The BOM aligns them. Only the app module needs it.
- **Debug signing.** The committed `skyemu-open-signing-store` keystore has no known passwords or alias in the
  repo, so it is not used. Each CI runner generates its own debug key, so CI APKs cannot update one another.
- **Uncompressed native libs** (`useLegacyPackaging = false`) keep 16 KB alignment and load straight from the
  APK; the price is the APK size.

## Reproduce

```sh
# SDK packages used (same as CI)
sdkmanager 'platforms;android-36' 'build-tools;36.0.0' 'ndk;28.2.13676358' 'cmake;3.22.1' 'platform-tools'

cd tools/android_project
export ANDROID_HOME=/path/to/sdk           # or local.properties
./gradlew :app:assembleRelease :apk:assembleRelease

APK=apk/build/outputs/apk/release/apk-release.apk
python3 ../verify_android_pages.py "$APK"
$ANDROID_HOME/build-tools/36.0.0/apksigner verify --print-certs "$APK"
$ANDROID_HOME/build-tools/36.0.0/aapt2 dump badging "$APK" | grep launchable
adb install -r "$APK"
```

Gotchas seen in this session:

- Maven Central answered `429 Too Many Requests` on the first dependency fetch. Retrying with a pause worked;
  later builds can use `--offline` once the Gradle cache is warm.
- A full native build for four ABIs takes about 5–6 minutes on 4 cores.

## Next steps

1. **Check the first CI run** of *Build Android* on `5758736`. If it is red, the likely spots are the new
   `:apk:assembleRelease` task (dependency resolution) and the second `verify_android_pages.py` call.
2. **Run the APK on a device or emulator**: launch from the home screen, open a ROM through **Load Game**,
   open a `.gba` from a file manager (file associations), and check Material You colors on Android 12+.
3. **Check the README on github.com**: the `[!TIP]` / `[!NOTE]` callouts, the Mermaid diagram and the badge
   buttons only render there.
4. **Open a pull request** when 1–3 pass (none exists yet).
5. Optional, larger:
   - Real release signing: keystore in GitHub secrets plus a `signingConfigs.release` read from environment
     variables, so CI APKs keep one identity and can update each other.
   - Smaller downloads: per-ABI splits (`splits { abi { … } }`) or an AAB for the Play Store
     (`tools/gen_playstore.py` exists for store assets).
   - Version from git tags instead of the hard-coded `versionCode 1`.
   - Publish the APK on a GitHub Release so the README's *Download APK* button can link straight to a file.

## Out of scope

`Pierreg99/KiBlox-VoxelGame` shares the branch name but was not changed. It is a browser game with no Android
target, and its README is generated by the owner's own template tooling.
