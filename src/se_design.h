/*****************************************************************************
 *
 *   SkyEmu design systems
 *
 *   Builds platform native design tokens (colors, shapes and metrics) for
 *   Material Design 3 (Android), Fluent / WinUI 3 (Windows 11) and
 *   libadwaita (GNOME). The tokens are consumed by the Dear ImGui front end
 *   in main.c. This file has no ImGui or sokol dependency so it can be unit
 *   tested on its own (see tools/se_design_test.c).
 *
**/
#ifndef SE_DESIGN_H
#define SE_DESIGN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Values of persistent_settings_t.design_system
#define SE_DESIGN_AUTO      0 // Native design language of the host platform
#define SE_DESIGN_CLASSIC   1 // Original SkyEmu image based skin
#define SE_DESIGN_MATERIAL3 2 // Material Design 3 / Material You (Android)
#define SE_DESIGN_FLUENT    3 // Fluent 2 / WinUI 3 (Windows 11)
#define SE_DESIGN_ADWAITA   4 // libadwaita (GNOME)
#define SE_DESIGN_COUNT     5

// Values of persistent_settings_t.color_scheme
#define SE_COLOR_SCHEME_SYSTEM 0 // Follow the OS light/dark preference
#define SE_COLOR_SCHEME_LIGHT  1
#define SE_COLOR_SCHEME_DARK   2
#define SE_COLOR_SCHEME_BLACK  3 // Dark with pure black surfaces (AMOLED)
#define SE_COLOR_SCHEME_COUNT  4

#define SE_ACCENT_NONE 0xffffffffu // No accent color is known

typedef struct{float r,g,b,a;}se_color_t;

// Standard Material tones, the tones the Android system palette exposes
// (system_accent1_0 is tone 100, system_accent1_1000 is tone 0).
#define SE_NUM_STANDARD_TONES 13
extern const float se_standard_tones[SE_NUM_STANDARD_TONES];

// A tonal palette is a hue/chroma pair that can be sampled at any tone.
// Tone is CIELAB L* (0 black, 100 white) like Material's HCT tone, so tone
// differences map to guaranteed contrast. Hue and chroma are OKLCH, which
// keeps hue stable across tones (plain CIELAB turns blues purple).
typedef struct{
  float hue;    // OKLCH hue in degrees
  float chroma; // OKLCH chroma (0 - ~0.37)
  bool has_exact;
  uint32_t exact[SE_NUM_STANDARD_TONES]; // 0xRRGGBB at se_standard_tones, e.g. read from Android
}se_tonal_palette_t;

typedef struct{
  se_tonal_palette_t primary, secondary, tertiary, neutral, neutral_variant;
}se_core_palette_t;

// Appearance reported by the operating system.
typedef struct{
  int dark;              // 1 dark, 0 light, -1 unknown
  uint32_t accent;       // 0xRRGGBB or SE_ACCENT_NONE
  bool has_core_palette; // Android 12+ Material You palette extracted from the wallpaper
  se_core_palette_t core_palette;
}se_system_appearance_t;

typedef struct{
  int design; // Resolved design system, never SE_DESIGN_AUTO or SE_DESIGN_CLASSIC
  bool dark;
  bool black;
  uint32_t accent; // Accent (or Material seed) the tokens were built from

  // Color roles use Material 3 names, the other design systems map their own
  // tokens onto the closest role.
  se_color_t primary, on_primary;                         // Accent fills: checked boxes, sliders, focus
  se_color_t primary_container, on_primary_container;     // Accent tinted containers (list icons)
  se_color_t secondary_container, on_secondary_container; // Selected items and toggles
  se_color_t tertiary_container, on_tertiary_container;   // Complementary highlight (hold toggle)
  se_color_t error, on_error;
  se_color_t accent_text;     // Accent color that is readable on the surfaces (links, section titles)
  se_color_t background;      // Behind the emulated screen
  se_color_t surface_bar;     // Top app bar / title bar / header bar
  se_color_t surface_panel;   // Side panels (navigation drawer / navigation pane / sidebar)
  se_color_t surface_content; // Main content sheets such as the game list
  se_color_t surface_card;    // Cards and grouped settings
  se_color_t surface_popup;   // Menus, combo popups and tooltips
  se_color_t surface_input;   // Text fields, combo boxes and slider tracks
  se_color_t on_surface, on_surface_variant, on_surface_disabled;
  se_color_t outline;         // Strong strokes: unchecked boxes, slider rails
  se_color_t outline_variant; // Dividers and card strokes
  se_color_t control, control_hover, control_active; // Neutral push buttons
  se_color_t selected, on_selected;                   // Selected toggle / segmented button
  se_color_t state_hover, state_press;                // Overlays for transparent widgets
  se_color_t scrim;

  // Shape, in logical pixels
  float panel_rounding, card_rounding, button_rounding, input_rounding;
  float check_rounding, popup_rounding, scrollbar_rounding;
  // Metrics, in logical pixels
  float frame_padding_x, frame_padding_y;
  float item_spacing_x, item_spacing_y;
  float window_padding;
  float scrollbar_size, grab_min_size;
  float control_border; // Border width of buttons and fields
  float popup_border;
  float check_border;   // Border width of an unchecked checkbox
  float font_size;
  float slider_track_h, slider_thumb_r;
  // Style switches
  bool section_accent;  // Section titles use the accent color (Material)
  bool section_bold;    // Section titles use a faux bold weight (Fluent, libadwaita)
  bool section_divider; // Draw a divider above section titles
  bool bar_divider;     // Draw a divider under the top bar
  bool thumb_ring;      // Slider thumb is a ring around an accent dot (Fluent)
  bool thumb_light;     // Slider thumb is a light knob (libadwaita)
}se_design_tokens_t;

// Design system selection
int se_design_platform_default(void);
int se_design_resolve(int design);
const char* se_design_name(int design);
bool se_design_is_dark(int color_scheme, const se_system_appearance_t* sys);
uint32_t se_design_default_accent(int design);
// Builds the tokens of design (not AUTO/CLASSIC). A custom_accent of SE_ACCENT_NONE uses the
// system accent, and falls back to the design's default accent when the system has none.
void se_design_build_tokens(int design, int color_scheme, uint32_t custom_accent,
                            const se_system_appearance_t* sys, se_design_tokens_t* out);

// Color science helpers
float se_color_tone(uint32_t rgb); // CIELAB L* of an sRGB color
float se_contrast_ratio(uint32_t a, uint32_t b); // WCAG contrast ratio
void se_tonal_palette_from_color(uint32_t rgb, se_tonal_palette_t* out);
void se_tonal_palette_from_hue_chroma(float hue, float chroma, se_tonal_palette_t* out);
// Palette from colors at se_standard_tones (Android's system_accent1_0 ... system_accent1_1000).
// Those tones are returned exactly, tones in between are generated.
void se_tonal_palette_from_tones(const uint32_t rgb_at_standard_tones[SE_NUM_STANDARD_TONES], se_tonal_palette_t* out);
uint32_t se_tonal_palette_tone(const se_tonal_palette_t* palette, float tone);
// Material 3 "tonal spot" palettes (the Android 12+ default) from a seed color.
void se_core_palette_from_seed(uint32_t seed, se_core_palette_t* out);
se_color_t se_color_from_rgb(uint32_t rgb, float alpha);
uint32_t se_color_to_rgb(se_color_t c);
se_color_t se_color_blend(se_color_t base, se_color_t overlay); // overlay composited over base

// Platform integration. Each is a no-op on platforms it does not apply to.
// Queries the desktop OS light/dark preference and accent color (Windows registry,
// GNOME settings). Android and UWP hosts push theirs through the public API instead.
void se_design_query_system_appearance(se_system_appearance_t* out);
// Matches the native window frame to the app: dark title bar and caption colors on
// Windows 11, and the GTK theme variant that GNOME Shell uses to draw X11 decorations.
// Pass SE_ACCENT_NONE as caption colors to keep the system defaults.
void se_design_style_native_window(const void* win32_hwnd, void* x11_display, unsigned long x11_window,
                                   bool dark, uint32_t caption_rgb, uint32_t caption_text_rgb);
// Finds the UI font of the platform (Segoe UI Variable, Roboto, Adwaita Sans/Cantarell).
// Only returns fonts Dear ImGui's stb_truetype can rasterize.
bool se_design_find_system_font(int design, char* path, size_t path_size);
bool se_design_font_is_supported(const uint8_t* data, size_t size);

#ifdef __cplusplus
}
#endif

#endif
