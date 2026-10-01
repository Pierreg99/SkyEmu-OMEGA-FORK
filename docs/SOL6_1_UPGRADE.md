# Sol6.1VeryHighAccurateCodexversion

This branch starts from `claude/emulator-material-v3-design-zb5s2j` at
`1313eb776ab80d006926d1b9558ac941e5352c09`. It adds configurable display effects,
accessibility options, runtime fixes and current platform build configurations.
The upstream GB, GBC, GBA and DS CPU/PPU timing implementations remain in use.
The branch name is not an accuracy certification.

## Display and appearance

Menu → Display Settings includes three additional shaders:

| ID | Shader | Controls |
|---|---|---|
| 5 | CRT Scanlines | Scanline strength, curvature, vignette |
| 6 | CRT Aperture Grille | Scanlines, RGB mask, curvature, vignette |
| 7 | Soft LCD | Scanlines and pixel mask |

Existing modes 0–4 keep their IDs. Brightness, saturation and contrast apply to
all modes. Reset Display Effects restores their default strengths without
changing the shader selection. Scanline and mask patterns fade at small output
sizes to reduce moire. CRT curvature samples a black border outside the screen.

Menu → GUI includes High Contrast, Comfortable/Compact/Touch control density,
and Corner Roundness. These apply to Material 3, Fluent and Adwaita. High Contrast
uses opaque surfaces and readable black/white text on accent fills. Touch density
increases framed controls to at least 48 logical pixels at the default font size;
existing icon controls and classic image skins retain their own sizing.

Host apps can call `se_set_display_effects`, `se_set_display_color`, and
`se_set_design_options` in `src/skyemu_dll.h`. Both Android host classes expose
matching `se_android_set_*` methods. Call host settings APIs on the emulator's
own thread, as with the existing API. The HTTP `/settings` and `/setting` endpoints
expose all new controls.

## Logic fixes

- Exact-size file readers close handles on every exit and reject short/error reads.
  General file reads accept an optional size output and handle empty files safely.
  Save operations propagate flush/close failures; settings writes are retried
  when saving fails.
- Audio availability no longer changes counters as a side effect. Libretro reads
  contiguous spans with correct wraparound, handles partial consumption and
  discards audio when the frontend disables it.
- Settings migration and validation live in a separate module. NaN, infinity,
  invalid enum values and invalid ports are sanitized before rendering. Version
  5 consumes reserved space and preserves the existing 1,024-byte ABI and offsets.
  Version 0–4 settings migrate; an unknown future version resets to defaults.
- Save-state host APIs ignore invalid slot indices and requests without a ROM.
- Color correction clamps negative values before gamma conversion.
- Download caching is restored from settings on every launch.
- Android locale queries use matching JNI UTF-8 acquire/release calls, close local
  references, terminate the output string and retain existing JVM attachments.
  Menu height correctly adds the native menu offset.

## Platform builds

Android targets API 36, keeps API 24 as its minimum, uses AGP 8.10.1,
Gradle 8.11.1, NDK 28.2 and CMake 3.22.1, and builds an AAR for all four ABIs.
NDK builds use flexible page sizes and 16 KB LOAD alignment. CI checks each
packaged native library. A host app must also use a recent Android Gradle plugin
(8.5.1 or newer) to package native libraries with correct APK zip alignment.
Imports use the system document provider and app-owned storage; broad storage
permissions are capped at older Android versions. API 30+ uses WindowInsets for
immersive bars and safe drawing areas. Final behavior still needs device testing.

Windows builds use one dynamic CRT, including the matching Debug CRT, across
the DLL and its dependencies. Compiler flags respect the selected build
configuration. CI builds with Ninja Multi-Config, the installed MSVC toolchain and SDK on
Windows 2022 and 2025 runners.
The output remains the Windows host DLL.

Linux uses OpenGL's portable CMake target and supports current system curl,
OpenSSL and SDL2 packages. CI covers Ubuntu 22.04 and 24.04. Display support
remains X11/XWayland; native Wayland is not added by this branch.

## Reproduce validation

```sh
cmake --preset runtime-tests
cmake --build --preset runtime-tests
ctest --preset runtime-tests

cmake --preset runtime-tests -DSKYEMU_ENABLE_SANITIZERS=ON
cmake --build --preset runtime-tests
ctest --preset runtime-tests

# With Linux development dependencies installed:
cmake --preset linux-release
cmake --build --preset linux-release --parallel
ctest --preset linux-release
cmake --build build/linux-release --target skyemu_libretro --parallel

# Windows, from a Visual Studio developer shell:
cmake --preset windows-release
cmake --build --preset windows-release --parallel
ctest --preset windows-release

# Android, JDK 17 and SDK available:
cd tools/android_project
./gradlew :app:assembleRelease --no-daemon
cd ../..
python3 tools/verify_android_pages.py tools/android_project/app/build/outputs/aar/app-release.aar
```

`python3 tools/generate_shaders.py` regenerates the checked-in GLSL, GLES, HLSL,
Metal and WebGPU code using the compiler bundled for this Sokol version.
`--check` verifies consistency. Use Linux x86_64, or pass `--compiler PATH` to a
compatible compiler. Generated HLSL and Metal sources compile at application
startup on those platforms; the generation step is not a GPU/device test.

Focused tests cover settings ABI/migration and invalid values, audio wraparound
including UINT32 overflow, empty and wrong-size files, optional size outputs,
failed flushes, appearance contrast and control metrics. During development the
Linux app and libretro core were built, tests passed with ASan/UBSan, and Blargg's
combined GB CPU instruction test reported all eleven tests passed. This does
not establish exhaustive game compatibility or improvements in cycle accuracy.
