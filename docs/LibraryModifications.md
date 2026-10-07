<sub>[SkyEmu OMEGA](../README.md) › [Docs](README.md) › Library modifications</sub>

# Library modifications

Third-party libraries are vendored in `src/`. Changes made to them are listed here so they can be carried over
when a library is updated.

| Library | Files | Change | Why |
|---|---|---|---|
| curl 8.5 | `src/curl/CMakeLists.txt` | Commented out `find_package(OpenSSL)` and removed the `OpenSSL::*` prefixes | SkyEmu links its own OpenSSL build |
| sokol_app | `src/sokol/sokol_app.h` | Added `sapp_x11_get_display()` and `sapp_x11_get_window()` | [`src/se_design.c`](../src/se_design.c) sets `_GTK_THEME_VARIANT` on the X11 window so GNOME draws matching light or dark decorations |
