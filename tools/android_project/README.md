<sub>[SkyEmu OMEGA](../../README.md) › [Docs](../../docs/README.md) › Android project</sub>

# SkyEmu for Android

This Gradle project builds SkyEmu for Android: the native emulator (`libSkyEmu.so`, compiled from the
repository's top-level `CMakeLists.txt`) and the Java classes that show and control it. The GUI uses
[Material 3](../../docs/DESIGN_SYSTEMS.md) by default, with Material You colors on Android 12 and later.

| Module | What it is | Output |
|---|---|---|
| `standalone` | The SkyEmu app, ready to install (application ID `com.sky.SkyEmu`) | `standalone/build/outputs/apk/release/SkyEmu-v32-release.apk` |
| `app` | The **Android library** with everything else, for the app and for host apps that embed SkyEmu | `app/build/outputs/aar/app-release.aar` |

## Requirements

| | |
|---|---|
| Android Gradle Plugin | 8.7.0 (Gradle 8.9 through the wrapper) |
| JDK | 17 |
| NDK | 28.2.13676358 |
| CMake | 3.22.1 |
| compileSdk / targetSdk | 35 |
| minSdk | 24 (Android 7.0) |
| ABIs | arm64-v8a, armeabi-v7a, x86, x86_64 |

## Build

```sh
cd tools/android_project
./gradlew :standalone:assembleRelease :app:assembleRelease
adb install standalone/build/outputs/apk/release/SkyEmu-v32-release.apk
```

Or open this folder in Android Studio, let it sync and run the `standalone` configuration. Gradle finds the
Android SDK through `ANDROID_HOME` or a `local.properties` file with `sdk.dir=...` (not checked in, Android Studio
writes it). `-PskyemuAbis=arm64-v8a` builds only one ABI, which is faster while developing. The release app is
signed with the open signing key in this folder, replace it with your own for distribution.

## What is inside

| Path | Contents |
|---|---|
| `standalone/build.gradle` | The app (`com.android.application`): application ID, version, signing |
| `standalone/src/main/AndroidManifest.xml` | Makes `EnhancedNativeActivity` the launcher activity |
| `app/build.gradle` | Library module (`com.android.library`, namespace `com.skyemu`) and the NDK / CMake setup |
| `app/consumer-rules.pro` | Keeps the classes native code calls through JNI when an app shrinks its code |
| `app/src/main/AndroidManifest.xml` | `EnhancedNativeActivity` (merged into the app or host app), file associations for `.gb`, `.gbc`, `.gba`, `.nds` and `.zip`, and the `skyemu://oauth` link used for sign-in |
| `app/src/main/java/com/sky/SkyEmu/EnhancedNativeActivity.java` | The `NativeActivity` that runs SkyEmu: controllers, keyboard, file picker, system theme |
| `app/src/main/java/com/sky/SkyEmu/MainSkyEmuObject.java` | JNI API for host apps: input, save states, screen and every setting |
| `app/src/main/res` | Launcher icons and strings |
| `app/src/main/cpp` | Left over from the Google NDK *Native Activity* sample this project started from (Apache 2.0); not part of the build |

How a host app talks to SkyEmu (the activity's methods, the `se_android_*` API and the callbacks native code
expects) is described in [Embedding › Android library](../../docs/EMBEDDING.md#android-library).
