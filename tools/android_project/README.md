<sub>[SkyEmu OMEGA](../../README.md) › [Docs](../../docs/README.md) › Android project</sub>

# SkyEmu for Android

This Gradle project builds SkyEmu for Android as an **Android library module**: the native emulator
(`libSkyEmu.so`, compiled from the repository's top-level `CMakeLists.txt`) plus the Java classes a host app uses
to show and control it. The GUI uses [Material 3](../../docs/DESIGN_SYSTEMS.md) by default, with Material You
colors on Android 12 and later.

## Requirements

| | |
|---|---|
| Android Gradle Plugin | 8.7.0 (Gradle 8.9 through the wrapper) |
| JDK | 17 |
| NDK | 28.2.13676358 |
| CMake | 3.18.1 |
| compileSdk / targetSdk | 35 |
| minSdk | 24 (Android 7.0) |
| ABIs | arm64-v8a, armeabi-v7a, x86, x86_64 |

## Build

```sh
cd tools/android_project
./gradlew assembleRelease
```

Or open this folder in Android Studio and let it sync. The release build is signed with the open signing key in
this folder, replace it with your own for distribution.

## What is inside

| Path | Contents |
|---|---|
| `app/build.gradle` | Library module (`com.android.library`, namespace `com.skyemu`) and the NDK / CMake setup |
| `app/src/main/AndroidManifest.xml` | `EnhancedNativeActivity`, file associations for `.gb`, `.gbc`, `.gba`, `.nds` and `.zip`, and the `skyemu://oauth` link used for sign-in |
| `app/src/main/java/com/sky/SkyEmu/EnhancedNativeActivity.java` | The `NativeActivity` that runs SkyEmu: controllers, keyboard, file picker, system theme |
| `app/src/main/java/com/sky/SkyEmu/MainSkyEmuObject.java` | JNI API for host apps: input, save states, screen and every setting |
| `app/src/main/res` | Launcher icons and strings |
| `app/src/main/cpp` | Left over from the Google NDK *Native Activity* sample this project started from (Apache 2.0); not part of the build |

How a host app talks to SkyEmu (the activity's methods, the `se_android_*` API and the callbacks native code
expects) is described in [Embedding › Android library](../../docs/EMBEDDING.md#android-library).
