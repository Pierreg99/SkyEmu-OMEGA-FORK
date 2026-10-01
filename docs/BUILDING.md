<sub>[SkyEmu OMEGA](../README.md) › [Docs](README.md) › Building</sub>

# Building SkyEmu OMEGA

Use CMake 3.21 or newer (Android uses 3.22.1). Everything builds with CMake. The same `CMakeLists.txt` drives the desktop builds, the Android NDK build, the
Xcode projects and the Emscripten web build.

| Platform | Output | Jump to |
|---|---|---|
| Linux, FreeBSD | `build/bin/SkyEmu` | [Linux](#linux) · [FreeBSD](#freebsd) |
| Windows | `build/bin/<Config>/SkyEmu.dll` | [Windows](#windows) |
| macOS | `build/bin/SkyEmu.app` or a static library | [macOS](#macos) |
| iOS | Static library | [iOS](#ios) |
| Android | `libSkyEmu.so` inside an Android library | [Android](#android) |
| Web | `SkyEmu.html` + WebAssembly | [Web](#web) |
| libretro | `skyemu_libretro` core | [libretro](#libretro-core) |

## CMake options

| Option | Default | Effect |
|---|---|---|
| `ENABLE_RETRO_ACHIEVEMENTS` | `ON` | RetroAchievements support; may be disabled for a smaller build. |
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
  libssl-dev libcurl4-openssl-dev libsdl2-dev   # only needed with USE_SYSTEM_OPENSSL / USE_SYSTEM_CURL

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

Windows builds produce **`SkyEmu.dll`**, not an executable. A host application loads it and starts the emulator
with `win_main(argc, argv)`, see [Embedding](EMBEDDING.md#windows-dll).

```bat
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_VERSION=10.0.19041.0
cmake --build build --config RelWithDebInfo
```

The DLL is written to `build\bin\RelWithDebInfo\`.

## macOS

```sh
cmake -B build && cmake --build build          # build/bin/SkyEmu.app
```

Pass `-DBUILD_MACOS_STATIC_LIB=ON` to get a static library for a host app instead. `gen_macos.sh` generates an
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

> [!WARNING]
> `-DBUILD_IOS_STATIC_LIB=OFF` generates an app bundle target, but it does not link on its own (undefined
> `_main`) for the same reason. This is also why the *Build iOS* workflow fails.

## Android

The Gradle project in `tools/android_project` builds SkyEmu with the NDK through the top-level `CMakeLists.txt`.
It is an **Android library module** (namespace `com.skyemu`) that host apps depend on; see the
[Android project page](../tools/android_project/README.md) and [Embedding](EMBEDDING.md#android-library).

| | |
|---|---|
| NDK | 28.2.13676358 |
| compileSdk / targetSdk | 36 |
| minSdk | 24 (Android 7.0) |
| ABIs | arm64-v8a, armeabi-v7a, x86, x86_64 |

```sh
cd tools/android_project
./gradlew assembleRelease
```

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

The design token engine has a stand-alone unit test, also run by the Linux workflow:

```sh
cc -O2 -Isrc tools/se_design_test.c src/se_design.c -lm -o se_design_test && ./se_design_test
```

The emulation cores can be checked against test ROMs with the `run_gb_test` and `run_gba_test` command line
modes of the executable.

CMake presets, shader regeneration, regression tests and platform changes are
described in [Sol6.1 upgrade notes](SOL6_1_UPGRADE.md).
