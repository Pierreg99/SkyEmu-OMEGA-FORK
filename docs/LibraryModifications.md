# Library modifications

## curl (v8.5)
    - commented out find_package(OpenSSL) & removed OpenSSL::* prefixes since we link with our own OpenSSL

## sokol_app.h
    - added sapp_x11_get_display() and sapp_x11_get_window() so the design system code (src/se_design.c)
      can set _GTK_THEME_VARIANT on the X11 window, which makes GNOME draw matching light/dark decorations
