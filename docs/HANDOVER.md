<sub>[SkyEmu OMEGA](../README.md) › [Docs](README.md) › Handover</sub>

# Handover: SkyEmu OMEGA for Android, and a refreshed README

A short summary for the project owner. The technical details, reasoning and reproduction steps are in
[HANDOFF.md](HANDOFF.md).

## What you now have

- **An installable Android app.** Until now the Android project only built a library for other apps to embed.
  It now also builds **SkyEmu** as an app you can put on a phone: one APK of about 106 MB, Android 7.0 and newer,
  for ARM and x86 devices, with its own home-screen icon.
- **Automatic APK builds.** Every push builds the APK on GitHub Actions. Download it as the
  **AndroidRelease-APK** artifact of a [Build Android](https://github.com/Pierreg99/SkyEmu-OMEGA-FORK/actions/workflows/deploy_android.yml)
  run.
- **A refreshed README**: a *Download APK* button, tidier badges, a one-line section menu, the light and dark
  design gallery, a diagram of how other apps embed SkyEmu, and step-by-step Android install instructions.

All of it is on branch `ccr-e41ac3c8-r3njjn` (commit `5758736`). No pull request has been opened.

## Installing it

1. Download **AndroidRelease-APK** from a green *Build Android* run and unzip it to get `apk-release.apk`.
2. Open it on the phone and allow installing from that app, or run `adb install apk-release.apk`.
3. Start **SkyEmu** and tap **Load Game**.

## Things to know

| | |
|---|---|
| ⚠️ **Not yet tried on a phone** | The APK was built and checked automatically, but nobody has run it on a device yet. |
| ⚠️ **Test signature** | The APK is signed with a throwaway test key. Android may warn about an unknown developer, and a newer build can't be installed over an older one without uninstalling first. Before publishing anywhere, it needs your own signing key. |
| ⏳ **First CI run** | The first automatic build with the APK step was still running at handover. |
| ℹ️ **Size** | About 106 MB because one file covers all four processor types. Separate per-device files would be smaller. |
| ℹ️ **KiBlox** | The KiBlox repository was not changed: it is a browser game with no Android version. |

## Decisions for you

1. **Merge?** Once CI is green and the app has been tried on a phone, open and merge a pull request.
2. **Signing:** do you want a permanent signing key set up in GitHub so every build can update the last one?
3. **Distribution:** keep downloads in Actions, or publish APKs on a GitHub Release (or the Play Store)?

## Where to look

| Need | Go to |
|---|---|
| Technical handoff | [docs/HANDOFF.md](HANDOFF.md) |
| Build instructions | [docs/BUILDING.md › Android](BUILDING.md#android) |
| Android project | [tools/android_project/README.md](../tools/android_project/README.md) |
| Builds and downloads | [GitHub Actions](https://github.com/Pierreg99/SkyEmu-OMEGA-FORK/actions) |
