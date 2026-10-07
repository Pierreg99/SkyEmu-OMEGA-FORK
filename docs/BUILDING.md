<sub>[SkyEmu OMEGA](../README.md) › [Docs](README.md) › Building</sub>

# Building SkyEmu OMEGA

Everything builds with CMake. The same `CMakeLists.txt` drives the desktop builds, the Android NDK build, the
Xcode projects and the Emscripten web build.

| Platform | Output | Jump to |
|---|---|---|
| Linux, FreeBSD | `build/bin/SkyEmu` | [Linux](#linux) · [FreeBSD](#freebsd) |
| Windows | `build/bin/<Config>/SkyEmu.exe` and `SkyEmu.dll` | [Windows](#windows) |
| macOS | `build/bin/SkyEmu.app` or a static library | [macOS](#macos) |
| iOS | Static library, or `SkyEmu.app` | [iOS](#ios) |
| Android | An APK, and `libSkyEmu.so` inside an Android library (AAR) | [Android](#android) |
| Web | `SkyEmu.html` + WebAssembly | [Web](#web) |
| libretro | `skyemu_libretro` core | [libretro](#libretro-core) |

## CMake options

| Option | Default | Effect |
|---|---|---|
| `ENABLE_RETRO_ACHIEVEMENTS` | `ON` | RetroAchievements support. Keep it on: turning it off currently fails to compile (`atlas_tile_t` is only declared with it). |
| `USE_SYSTEM_CURL` | `OFF` | Link the system libcurl instead of building the bundled one |
| `USE_SYSTEM_OPENSSL` | `OFF` | Link the system OpenSSL instead of building the bundled one |
| `USE_SDL` | `ON` | SDL2 for game controllers and rumble on desktop |
| `USE_SYSTEM_SDL2` | `OFF` | Link the system SDL2 instead of the bundled one |
| `RETRO_CORE_ONLY` | `OFF` | Only configure the libretro core |
| `BUILD_IOS_STATIC_LIB` | `ON` | On iOS, build a static library instead of an app bundle |
| `BUILD_MACOS_STATIC_LIB` | `OFF` | On macOS, build a static library instead of `SkyEmu.app` |

> [!TIP]
> Building the bundled curl and OpenSSL takes most of the build time. On Linux, install their development
> packages and pass `-DUSE_SYSTEM_CURL=ON -DUSE_SYSTEM_OPENSSL=ON` to cut a clean build to about a minute on
> a 4 core machine.

## Linux

```sh
# Debian / Ubuntu
sudo apt install build-essential cmake ninja-build \
  libx11-dev libxi-dev libxrandr-dev libxinerama-dev libxcursor-dev \
  libgl1-mesa-dev libegl1-mesa-dev libasound2-dev \
  libssl-dev libcurl4-openssl-dev   # only needed with USE_SYSTEM_OPENSSL / USE_SYSTEM_CURL

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DUSE_SYSTEM_CURL=ON -DUSE_SYSTEM_OPENSSL=ON
cmake --build build
./build/bin/SkyEmu path/to/game.gba
```

SkyEmu uses X11 and runs under XWayland on Wayland desktops. On GNOME it picks up the color scheme, the accent
color and the interface font (see [Design systems](DESIGN_SYSTEMS.md)).

## FreeBSD

```sh
pkg install cmake libX11 libXi libXrandr libXinerama libXcursor libglvnd alsa-lib
cmake -B build && cmake --build build
```

## Windows

The emulator is built as **`SkyEmu.dll`**, with a small **`SkyEmu.exe`** next to it that starts it, so SkyEmu runs
on its own like on other platforms. Host applications load the DLL themselves and call `win_main(argc, argv)`,
see [Embedding](EMBEDDING.md#windows-dll).

```bat
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_VERSION=10.0.19041.0
cmake --build build --config RelWithDebInfo
build\bin\RelWithDebInfo\SkyEmu.exe path\to\game.gba
```

`SkyEmu.exe`, `SkyEmu.dll` and their debug symbols are written to `build\bin\RelWithDebInfo\`. Keep the two files
together; the `WindowsRelease` artifact of the *Build Windows* workflow contains both.

## macOS

```sh
cmake -B build && cmake --build build --parallel     # build/bin/SkyEmu.app
open build/bin/SkyEmu.app
```

The app draws with Metal and uses SDL for game controllers. Pass `-DBUILD_MACOS_STATIC_LIB=ON` to get a static
library for a host app instead; the app bundle adds only [`src/apple_launcher.c`](../src/apple_launcher.c), whose
`main()` calls the library's `main_macos()`. The *Build macOS* workflow packages the app as `SkyEmu.dmg`. `gen_macos.sh` generates an
Xcode project (run it from an empty build directory); the checked-in `host_app_macos/` project was generated
the same way.

## iOS

The iOS build is a static library by default. The helper scripts are meant to be run from an empty build
directory inside the repository:

| Script | What it does |
|---|---|
| `gen_ios.sh` | Generates the Xcode project for devices (iOS 15+) |
| `gen_ios_sim.sh` | Generates the Xcode project for the simulator (Metal backend) |
| `build_static_ios.sh` | Archives the static library for devices with `xcodebuild` |
| `build_static_ios_sim.sh` | Archives the static library for the simulator |

```sh
mkdir build-ios && cd build-ios
../gen_ios.sh
../build_static_ios.sh
```

The library has no `main()`: in this fork sokol's iOS entry point is renamed `main_ios()`, and the host app calls
it (see [Embedding › iOS and macOS](EMBEDDING.md#ios-and-macos)).

`-DBUILD_IOS_STATIC_LIB=OFF` builds the SkyEmu app instead, with [`src/apple_launcher.c`](../src/apple_launcher.c)
as its `main()`. The *Build iOS* workflow does this and uploads an unsigned `SkyEmu.ipa`, to sign with your own
certificate (or sideload with a tool that signs it):

```sh
cmake -B build-ios -GXcode -DCMAKE_SYSTEM_NAME=iOS -DBUILD_IOS_STATIC_LIB=OFF
cmake --build build-ios --config Release      # build-ios/bin/Release/SkyEmu.app
```

## Android

The Gradle project in `tools/android_project` builds SkyEmu with the NDK through the top-level `CMakeLists.txt`.
It has two modules:

| Module | Builds | Output |
|---|---|---|
| `standalone` | The SkyEmu app, ready to install | `standalone/build/outputs/apk/release/SkyEmu-v32-release.apk` |
| `app` | The **Android library** (namespace `com.skyemu`) that the app and host apps embed | `app/build/outputs/aar/app-release.aar` |

See the [Android project page](../tools/android_project/README.md) and [Embedding](EMBEDDING.md#android-library).

| | |
|---|---|
| NDK | 28.2.13676358 |
| compileSdk / targetSdk | 35 |
| minSdk | 24 (Android 7.0) |
| ABIs | arm64-v8a, armeabi-v7a, x86, x86_64 |

```sh
cd tools/android_project
./gradlew :standalone:assembleRelease :app:assembleRelease
adb install standalone/build/outputs/apk/release/SkyEmu-v32-release.apk
```

Add `-PskyemuAbis=arm64-v8a` to build only the ABI of your device, which is faster. The SDK location comes from
`ANDROID_HOME` or a `local.properties` file (`sdk.dir=...`), which is not checked in. The *Build Android*
workflow uploads the app as `AndroidRelease` and the library as `AndroidLibrary`.

## Web

```sh
# with emsdk 4.0.7 activated
emcmake cmake -B build -DPLATFORM=Web
cmake --build build
# serve build/bin/ (SkyEmu.html, SkyEmu.js, SkyEmu.wasm) with any static web server
```

The HTTP control server is not available in the web build.

## libretro core

```sh
cmake -B build -DRETRO_CORE_ONLY=ON
cmake --build build --target skyemu_libretro --config Release
```

## Nix

```sh
nix build           # package
nix run             # build and start SkyEmu
```

## Tests

The design token engine, the ROM patch engine, the cheat finder, the recorder and the DS screen layouts have
stand-alone unit tests, run by the *Unit tests* workflow on every push:

```sh
cc -O2 -Isrc tools/se_design_test.c src/se_design.c -lm -o se_design_test && ./se_design_test
cc -O2 -Isrc tools/se_patch_test.c src/se_patch.c -o se_patch_test && ./se_patch_test
cc -O2 -Isrc tools/se_cheat_finder_test.c src/se_cheat_finder.c -o se_cheat_finder_test && ./se_cheat_finder_test
cc -O2 -Isrc tools/se_record_test.c src/se_record.c src/stb.c -lm -o se_record_test && ./se_record_test
cc -O2 -Isrc tools/se_screen_layout_test.c src/se_screen_layout.c -lm -o se_screen_layout_test && ./se_screen_layout_test
```

The emulation cores can be checked against test ROMs with the `run_gb_test` and `run_gba_test` command line
modes of the executable.
