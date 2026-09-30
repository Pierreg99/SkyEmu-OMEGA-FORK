# Platform design systems

SkyEmu's GUI can follow the native design language of the platform it runs on:

| Platform | Default design | Light/dark from | Accent from | UI font |
|---|---|---|---|---|
| Android | Material 3 / Material You | System dark theme | Wallpaper colors (Android 12+) | Roboto |
| Windows | Fluent (Windows 11 / WinUI 3) | Default app mode (light/dark) | Windows accent color | Segoe UI Variable (Segoe UI on Windows 10) |
| Linux, FreeBSD | Adwaita (GNOME / libadwaita) | Desktop portal or GNOME "Style" | GNOME 47+ accent color | GNOME interface font (Adwaita Sans, Cantarell) |
| Web | Material 3 | Browser `prefers-color-scheme` | Default accent | Bundled font |
| macOS, iOS | SkyEmu Classic | | | |

![Material 3, Fluent and Adwaita in dark and light](design-systems.png)

Every design is available on every platform. They are selected in **Menu → GUI → Design**:

- **Platform Native**: the design from the table above (default).
- **SkyEmu Classic**: the original image based skin, including its Dark/Light/Black themes and custom skins.
- **Material 3**, **Fluent (Windows 11)**, **Adwaita (GNOME)**.

With a platform design selected the GUI section also offers:

- **Color Scheme**: Follow System, Light, Dark or Black (AMOLED, pure black surfaces).
- **Custom Accent Color**: overrides the system accent with a preset or any color. Material 3 derives
  its whole tonal scheme from it, like Android does from a wallpaper.
- **Use System Font**: uses the platform UI font when it is installed and readable, otherwise the bundled font.

Settings written by earlier versions are migrated: a custom skin keeps the classic design, and the
Light and Black themes carry over as the color scheme.

## What changes

The design systems restyle every Dear ImGui surface with the tokens of the chosen design:

- Top bar: Material top app bar, Windows title bar on Mica, GNOME header bar with its bottom shade.
- The rewind / slow / play / fast forward toggles become a Material segmented button, or linked toggles on Fluent and Adwaita.
- Buttons, text fields, combo boxes, checkboxes and sliders use the shapes, strokes and state layers of each design
  (e.g. pill buttons and 2dp checkboxes on Material, 4px corners and 1px strokes on Fluent, flat 6px buttons on Adwaita).
- Section titles use the Material primary color, or bold text on Fluent and Adwaita.
- The navigation (Menu) panel, the game list sheet and cards use the matching surface roles.
- The window frame follows the GUI: dark title bar and caption colors on Windows 11 (DWM), and the
  `_GTK_THEME_VARIANT` window property that GNOME Shell uses for light/dark decorations.
- On-screen touch controls are drawn as tonal buttons with a rounded d-pad that fill with the accent when pressed.
  Note that this fork currently keeps the on-screen controller disabled (`touch_controller_active` in
  `se_draw_emulated_system_screen`), so they only show once it is enabled again.

SkyEmu Classic renders exactly as before.

## Color and shape tokens

`src/se_design.c` builds the tokens and has no ImGui dependency.

- **Material 3** uses the "tonal spot" scheme of Android 12+. Tones are CIELAB L\* (the tone of Material's
  HCT color space) and hue/chroma are OKLCH, with chroma clipped to the sRGB gamut per tone. This
  reproduces the Material 3 baseline palette within a few 8 bit units per channel (primary40 is exact)
  without porting the full HCT solver. On Android 12+ the 65 system palette colors
  (`system_accent1_0` ... `system_neutral2_1000`) are used directly.
- **Fluent** uses the WinUI 3 theme resources: Mica base, layer and card fills, control fills and strokes,
  and `SystemAccentColorLight2` / `SystemAccentColorDark1` as accent fill in dark / light mode.
- **Adwaita** uses the libadwaita 1.6 palette (window, header bar, sidebar, card and popover colors, widgets
  tinted with `alpha(currentColor, x)`) and computes `accent_color` from `accent_bg_color` the way libadwaita does.

The unit test checks tones, the Material baseline, the Android palette path and that text stays readable
(7:1 for body text, 3:1 on accent fills) for every design, color scheme and a set of accents:

```
cc -O2 -Isrc tools/se_design_test.c src/se_design.c -lm -o se_design_test && ./se_design_test
```

## Integration APIs

### C / Windows DLL (`src/skyemu_dll.h`)

```c
se_set_design_system(uint32_t design);   // 0 native, 1 classic, 2 Material 3, 3 Fluent, 4 Adwaita
se_set_color_scheme(uint32_t scheme);    // 0 system, 1 light, 2 dark, 3 black
se_set_accent_color(uint32_t rgb);       // 0xRRGGBB, or 0xFFFFFFFF to follow the system
se_set_system_appearance(int dark, uint32_t accent_rgb); // host reported appearance
```

and the matching getters. SkyEmu reads the Windows registry itself. Hosts that can not grant that access
(e.g. a packaged UWP/WinUI app) report the appearance instead, and again whenever it changes:

```csharp
[DllImport("SkyEmu.dll")] static extern void se_set_system_appearance(int dark, uint accentRgb);

var ui = new Windows.UI.ViewManagement.UISettings();
void Report(){
    var bg = ui.GetColorValue(UIColorType.Background);
    var a = ui.GetColorValue(UIColorType.Accent);
    se_set_system_appearance(bg.R < 128 ? 1 : 0, (uint)(a.R << 16 | a.G << 8 | a.B));
}
Report();
ui.ColorValuesChanged += (s, e) => Report();
```

### Android

`EnhancedNativeActivity.getSystemAppearance()` is called from native code for the dark mode flag and the
Material You palettes. The activity handles `uiMode` configuration changes, so switching the system theme
restyles the GUI without restarting the game. `MainSkyEmuObject` exposes the settings to host apps:

```java
se_android_set_design_system(int design);
se_android_set_color_scheme(int scheme);
se_android_set_accent_color(int rgb);                  // -1 follows the system
se_android_set_system_appearance(int dark, int accent); // for hosts with their own activity
```

### HTTP control server

`/setting` accepts `design`, `color_scheme`, `accent` (hex `RRGGBB` or `system`) and `system_font` (0/1),
e.g. `http://localhost:8080/setting?design=2&color_scheme=1&accent=3584e4`. `/settings` reports
`design_system`, `color_scheme`, `use_custom_accent`, `custom_accent` and `use_bundled_font`.

## Notes

- Dear ImGui renders a single font weight, bold titles are drawn with a one point offset second pass.
- On Linux the appearance is read with `gdbus` (desktop portal) and `gsettings`. To avoid spawning
  processes during gameplay it is refreshed only while the emulator is not running a game.
- Fonts are validated before they are handed to Dear ImGui (TrueType or CFF outlines with a Unicode cmap).
  CFF2 variable fonts such as `Cantarell-VF.otf` are skipped in favor of the next candidate.
