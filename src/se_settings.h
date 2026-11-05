#ifndef SE_SETTINGS_H
#define SE_SETTINGS_H
#include <stdint.h>
#include <stdbool.h>
#define SE_SETTINGS_VERSION 7u
#define SE_AVOID_OVERLAP_PORTRAIT 1
#define SE_AVOID_OVERLAP_LANDSCAPE 2
// Groups of on-screen buttons that the touch layout editor moves and resizes together
#define SE_TOUCH_GROUP_DPAD 0
#define SE_TOUCH_GROUP_FACE 1     // A, B, X, Y
#define SE_TOUCH_GROUP_SHOULDER 2 // L, R
#define SE_TOUCH_GROUP_SYSTEM 3   // Start, Select, Hold, Turbo
#define SE_TOUCH_GROUP_COUNT 4
#define SE_SHADER_PIXELATE 0u
#define SE_SHADER_BILINEAR 1u
#define SE_SHADER_LCD 2u
#define SE_SHADER_SUBPIXELS 3u
#define SE_SHADER_XBRZ 4u
#define SE_SHADER_CRT 5u
#define SE_SHADER_CRT_APERTURE 6u
#define SE_SHADER_SOFT_LCD 7u
#define SE_SHADER_ULTRA_CRT 8u
#define SE_SHADER_GB_LCD 9u
#define SE_SHADER_PIXEL_SMOOTH 10u
#define SE_SHADER_GBA_LCD 11u
#define SE_SHADER_COUNT 12u
typedef struct{
  // This structure is directly saved out for the user settings. 
  // Be very careful to keep alignment and ordering the same otherwise you will break the settings. 
  uint32_t draw_debug_menu;
  float volume; 
  uint32_t theme; 
  uint32_t settings_file_version; 
  uint32_t gb_palette[4];
  float ghosting;
  float color_correction;
  uint32_t integer_scaling; 
  uint32_t screen_shader; // SE_SHADER_*
  uint32_t screen_rotation; //0: No rotation, 1: Rotate Left, 2: Rotate Right, 3: Upside Down
  uint32_t stretch_to_fit;
  uint32_t auto_hide_touch_controls;
  float touch_controls_opacity; 
  uint32_t always_show_menubar;
  uint32_t language;
  float touch_controls_scale; 
  uint32_t touch_controls_show_turbo; 
  uint32_t save_to_path;
  uint32_t force_dmg_mode; 
  uint32_t gba_color_correction_mode; // 0 = SkyEmu, 1 = Higan
  uint32_t http_control_server_port; 
  uint32_t http_control_server_enable;
  uint32_t avoid_overlaping_touchscreen; // 1=Avoid Overlap in Portrait, 2=Avoid Overlap in Landscape, 3=Avoid Overlap in Both
  float custom_font_scale;
  uint32_t hardcore_mode;
  uint32_t draw_challenge_indicators;
  uint32_t draw_progress_indicators;
  uint32_t draw_leaderboard_trackers;
  uint32_t draw_notifications;
  float gui_scale_factor;
  uint32_t only_one_notification;
  uint32_t enable_download_cache;
  uint32_t nds_layout; 
  uint32_t touch_screen_show_button_labels;
  uint32_t show_screen_bezel;
  uint32_t design_system;     // SE_DESIGN_* (0 = native design of the platform)
  uint32_t color_scheme;      // SE_COLOR_SCHEME_* (0 = follow the system)
  uint32_t use_custom_accent; // 0 = system accent (Material You, Windows, GNOME), 1 = custom_accent
  uint32_t custom_accent;     // 0xRRGGBB
  uint32_t use_bundled_font;  // 0 = use the platform UI font when it is available, 1 = bundled font
  // Version 5 additions consume reserved space; previous field offsets stay fixed.
  float scanline_strength, mask_strength, curvature, vignette;
  float display_brightness, display_saturation, display_contrast;
  uint32_t high_contrast;
  uint32_t ui_density; // 0 comfortable, 1 compact, 2 touch
  float corner_radius_scale;
  // Version 7: per group offset (x, y, in controller widths and heights) and size of the touch controls
  float touch_layout[SE_TOUCH_GROUP_COUNT][3];
  uint32_t padding[191];
}persistent_settings_t; 
#ifdef __cplusplus
static_assert(sizeof(persistent_settings_t)==1024, "settings ABI must remain 1024 bytes");
#else
_Static_assert(sizeof(persistent_settings_t)==1024, "settings ABI must remain 1024 bytes");
#endif

#ifdef __cplusplus
extern "C" {
#endif
// Upgrade loaded settings, or reset an unknown version. Does not access files or graphics.
void se_settings_migrate(persistent_settings_t* settings, bool mobile);
void se_settings_reset_touch_layout(persistent_settings_t* settings);
void se_settings_validate(persistent_settings_t* settings);
#ifdef __cplusplus
}
#endif
#endif
