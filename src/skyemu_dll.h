#ifndef SKYEMU_DLL_H
#define SKYEMU_DLL_H

#include <stdint.h>
#include <stdbool.h>

/*
 * SkyEmu DLL API header
 *
 * When building the DLL, define SKYEMU_DLL_EXPORTS so that functions are
 * exported with __declspec(dllexport).  When consuming the DLL from C/C++
 * code, include this header without defining SKYEMU_DLL_EXPORTS so that
 * functions are imported with __declspec(dllimport).
 *
 * For non-MSVC compilers on Windows (MinGW/GCC) we use __attribute__((visibility("default"))).
 * On non-Windows platforms the macros expand to nothing.
 */

/*
 * SKYEMU_API expands to dllexport/dllimport only when SE_PLATFORM_WINDOWS_DLL
 * is defined.  In all other build configurations it expands to nothing so that
 * the existing executable/library builds are unaffected.
 */
#if defined(SE_PLATFORM_WINDOWS_DLL)
typedef void(__stdcall* RemoteKeycodeCallback)(const char* data1, const char* data2);
typedef void(__stdcall* PingCallback)(void);
typedef void(__stdcall* ExternalMenuCallback)(void);
  #if defined(SKYEMU_DLL_EXPORTS)
    #define SKYEMU_API __declspec(dllexport)
  #else
    #define SKYEMU_API __declspec(dllimport)
  #endif
#else
  #define SKYEMU_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Load an HTML file (used for cloud/HTTP control) */
SKYEMU_API void se_load_html(const char *filename);

/* Load a ROM file by path */
SKYEMU_API void se_load_rom(const char *filename);

/* Show the emulator UI overlay */
SKYEMU_API void se_show_ui(void);

/*
 * ROM patches (IPS, UPS, BPS). A patch named like the ROM next to it, next to its save file or in
 * the patch path is applied in memory when the game loads; the ROM file is never changed.
 * se_load_patch copies a patch next to the save file of the loaded game and reloads the game,
 * returning true when the patch was applied. se_get_patch_status describes the patch in use or
 * why it could not be applied (empty when the game has no patch).
 */
SKYEMU_API bool se_load_patch(const char *patch_path);
SKYEMU_API const char* se_get_patch_status(void);
SKYEMU_API void se_set_soft_patching(int enabled);
SKYEMU_API int se_get_soft_patching(void);

/*
 * Cheats of the loaded game (Action Replay for the GBA and DS, GameShark for the Game Boy), saved
 * in its .code file. Indices go from 0 to 127. se_add_cheat takes the code as hex digits, spaces
 * and line breaks are ignored, and returns the new index or -1. se_get_cheats_json returns
 * [{"id", "name", "enabled", "code"}, ...], valid until its next call. Cheats are refused in
 * RetroAchievements Hardcore Mode.
 */
SKYEMU_API int se_add_cheat(const char* name, const char* code, int enabled);
SKYEMU_API bool se_remove_cheat(int index);
SKYEMU_API bool se_set_cheat_enabled(int index, int enabled);
SKYEMU_API const char* se_get_cheats_json(void);

/*
 * Cheat finder: finds where the game keeps a value by searching its RAM while the value changes.
 * se_cheat_search_start takes the value size (1, 2 or 4 bytes). compare is 0 equal, 1 not equal,
 * 2 greater, 3 less, 4 changed, 5 unchanged, 6 increased, 7 decreased, 8 increased by,
 * 9 decreased by (value is only used by 0-3, 8 and 9). se_cheat_search_filter returns the number
 * of addresses left. se_make_cheat adds an enabled code that keeps value at address and returns
 * its index or -1; se_cheat_search_add_code does the same with the size of the search.
 * se_get_cheat_search_json returns {"active", "value_size", "signed", "searches", "count",
 * "first", "results": [{"address", "value", "previous"}, ...]}, valid until its next call.
 */
SKYEMU_API bool se_cheat_search_start(int value_size, int is_signed);
SKYEMU_API uint32_t se_cheat_search_filter(int compare, uint32_t value);
SKYEMU_API uint32_t se_cheat_search_count(void);
SKYEMU_API bool se_cheat_search_get_result(uint32_t index, uint32_t* address, uint32_t* value);
SKYEMU_API void se_cheat_search_reset(void);
SKYEMU_API int se_cheat_search_add_code(uint32_t address, uint32_t value, const char* name);
SKYEMU_API int se_make_cheat(uint32_t address, uint32_t value, int value_size, const char* name);
SKYEMU_API const char* se_get_cheat_search_json(uint32_t first_result, uint32_t max_results);

/* Hide the emulator UI overlay */
SKYEMU_API void se_hide_ui(void);

/* Set stretch-to-fit mode: 0 = off, 1 = on */
SKYEMU_API void se_stretch_to_fit(int fit);

/*
 * Screen shader modes:
 *   0 = Pixelate (nearest-neighbor)
 *   1 = Bilinear
 *   2 = LCD filter
 *   3 = LCD & Subpixels
 *   4 = Smooth Upscale (xBRZ)
 *   5 = CRT (scanlines and aperture grille)
 *   6 = Scanlines
 */
SKYEMU_API void se_set_screen_shader(uint32_t shader_mode);
SKYEMU_API uint32_t se_get_screen_shader(void);

/*
 * Persistent Settings API
 * Getters and setters for all gui_state.settings fields.
 * Use these from Windows, macOS, iOS, or Android (via JNI).
 */

/* Volume (0.0 - 1.0) */
SKYEMU_API void se_set_volume(float volume);
SKYEMU_API float se_get_volume(void);

/* Theme index */
SKYEMU_API void se_set_theme(uint32_t theme);
SKYEMU_API uint32_t se_get_theme(void);

/*
 * Design system of the GUI:
 *   0 = Platform native (Material 3 on Android, Fluent on Windows, Adwaita on Linux,
 *       the classic skin on Apple platforms)
 *   1 = SkyEmu classic (image skin, uses the theme index above)
 *   2 = Material 3 / Material You
 *   3 = Fluent (Windows 11)
 *   4 = Adwaita (GNOME)
 */
SKYEMU_API void se_set_design_system(uint32_t design);
SKYEMU_API uint32_t se_get_design_system(void);

/* Color scheme of the design systems: 0 = follow system, 1 = light, 2 = dark, 3 = black (AMOLED) */
SKYEMU_API void se_set_color_scheme(uint32_t scheme);
SKYEMU_API uint32_t se_get_color_scheme(void);

/* Contrast of the design systems: 0 = follow system, 1 = standard, 2 = high */
SKYEMU_API void se_set_contrast(uint32_t contrast);
SKYEMU_API uint32_t se_get_contrast(void);

/* Accent color 0xRRGGBB, or 0xFFFFFFFF to follow the system accent (Material You, Windows, GNOME) */
SKYEMU_API void se_set_accent_color(uint32_t rgb);
SKYEMU_API uint32_t se_get_accent_color(void);

/*
 * Lets a host app report the appearance of the OS when SkyEmu can not read it itself,
 * e.g. a UWP/WinUI host passing UISettings values, or an Android host with its own activity.
 *   dark:       1 = dark, 0 = light, -1 = unknown
 *   accent_rgb: 0xRRGGBB, or 0xFFFFFFFF if unknown
 */
SKYEMU_API void se_set_system_appearance(int dark, uint32_t accent_rgb);

/* Lets a host app report the system high contrast setting (e.g. UWP AccessibilitySettings.HighContrast):
   1 = on, 0 = off, -1 = unknown (SkyEmu queries the system itself) */
SKYEMU_API void se_set_system_high_contrast(int high_contrast);

/* GB palette colors (index 0-3) */
SKYEMU_API void se_set_gb_palette(int index, uint32_t color);
SKYEMU_API uint32_t se_get_gb_palette(int index);

/* Ghosting effect strength (0.0 - 1.0) */
SKYEMU_API void se_set_ghosting(float ghosting);
SKYEMU_API float se_get_ghosting(void);

/* Color correction strength (0.0 - 1.0) */
SKYEMU_API void se_set_color_correction(float value);
SKYEMU_API float se_get_color_correction(void);

/* Integer scaling: 0 = off, 1 = on */
SKYEMU_API void se_set_integer_scaling(uint32_t value);
SKYEMU_API uint32_t se_get_integer_scaling(void);

/* Screen rotation: 0=None, 1=Left, 2=Right, 3=Upside Down */
SKYEMU_API void se_set_screen_rotation(uint32_t rotation);
SKYEMU_API uint32_t se_get_screen_rotation(void);

/* Stretch to fit getter (setter already exists as se_stretch_to_fit) */
SKYEMU_API uint32_t se_get_stretch_to_fit(void);

/* Auto-hide touch controls: 0 = off, 1 = on */
SKYEMU_API void se_set_auto_hide_touch_controls(uint32_t value);
SKYEMU_API uint32_t se_get_auto_hide_touch_controls(void);

/* Touch controls opacity (0.0 - 1.0) */
SKYEMU_API void se_set_touch_controls_opacity(float opacity);
SKYEMU_API float se_get_touch_controls_opacity(void);

/* Always show menubar: 0 = off, 1 = on */
SKYEMU_API void se_set_always_show_menubar(uint32_t value);
SKYEMU_API uint32_t se_get_always_show_menubar(void);

/* Language index */
SKYEMU_API void se_set_language_int(uint32_t language);
SKYEMU_API uint32_t se_get_language_int(void);

/* Touch controls scale factor */
SKYEMU_API void se_set_touch_controls_scale(float scale);
SKYEMU_API float se_get_touch_controls_scale(void);

/* Show turbo on touch controls: 0 = off, 1 = on */
SKYEMU_API void se_set_touch_controls_show_turbo(uint32_t value);
SKYEMU_API uint32_t se_get_touch_controls_show_turbo(void);

/* On-screen touch controller: 1 = shown (after the screen is touched, or always when
   "Hide when inactive" is off), 0 = never shown */
SKYEMU_API void se_set_touch_controller(int shown);
SKYEMU_API int se_get_touch_controller(void);
/* Rewind and Fast Forward buttons on the on-screen controller: 0 = hidden, 1 = shown */
SKYEMU_API void se_set_touch_controls_show_speed(uint32_t value);
SKYEMU_API uint32_t se_get_touch_controls_show_speed(void);
/* Restores the default portrait and landscape layouts of the on-screen controller */
SKYEMU_API void se_reset_touch_layout(void);

/* Face buttons of game controllers: 0 = the controller's A button is A (labels),
   1 = A is the right face button and B the bottom one (GBA / DS positions).
   Changing it rebinds the face buttons of the connected controller. */
SKYEMU_API void se_set_controller_face_layout(uint32_t layout);
SKYEMU_API uint32_t se_get_controller_face_layout(void);

/* Save game data to ROM path: 0 = off, 1 = on */
SKYEMU_API void se_set_save_to_path(uint32_t value);
SKYEMU_API uint32_t se_get_save_to_path(void);

/* Force DMG mode for Color GB: 0 = off, 1 = on */
SKYEMU_API void se_set_force_dmg_mode(uint32_t value);
SKYEMU_API uint32_t se_get_force_dmg_mode(void);

/* GBA color correction mode: 0 = SkyEmu, 1 = Higan */
SKYEMU_API void se_set_gba_color_correction_mode(uint32_t mode);
SKYEMU_API uint32_t se_get_gba_color_correction_mode(void);

/* HTTP control server port */
SKYEMU_API void se_set_http_control_server_port(uint32_t port);
SKYEMU_API uint32_t se_get_http_control_server_port(void);

/* HTTP control server enable: 0 = off, 1 = on */
SKYEMU_API void se_set_http_control_server_enable(uint32_t value);
SKYEMU_API uint32_t se_get_http_control_server_enable(void);

/* Avoid overlapping touchscreen: 1=Portrait, 2=Landscape, 3=Both, 0=Off */
SKYEMU_API void se_set_avoid_overlapping_touchscreen(uint32_t value);
SKYEMU_API uint32_t se_get_avoid_overlapping_touchscreen(void);

/* Custom font scale factor */
SKYEMU_API void se_set_custom_font_scale(float scale);
SKYEMU_API float se_get_custom_font_scale(void);

/* Hardcore mode (achievements): 0 = off, 1 = on */
SKYEMU_API void se_set_hardcore_mode(uint32_t value);
SKYEMU_API uint32_t se_get_hardcore_mode(void);

/*
 * RetroAchievements. se_ra_login starts logging in (the token is saved, so it only has to be done
 * once) and se_ra_get_login_state returns 0 logged out, 1 logging in or 2 logged in;
 * se_ra_get_login_error explains a failed login. Unofficial achievements are loaded and listed
 * too when enabled. In spectator mode unlocks and leaderboard entries are shown but not sent.
 * se_get_achievements_json returns the user, the game, its rich presence and every achievement
 * with its unlock state and progress, valid until its next call.
 */
SKYEMU_API void se_ra_login(const char* username, const char* password);
SKYEMU_API void se_ra_logout(void);
SKYEMU_API int se_ra_get_login_state(void);
SKYEMU_API const char* se_ra_get_login_error(void);
SKYEMU_API void se_set_ra_unofficial(int enabled);
SKYEMU_API int se_get_ra_unofficial(void);
SKYEMU_API void se_set_ra_spectator(int enabled);
SKYEMU_API int se_get_ra_spectator(void);
SKYEMU_API const char* se_get_achievements_json(void);

/* Draw challenge indicators: 0 = off, 1 = on */
SKYEMU_API void se_set_draw_challenge_indicators(uint32_t value);
SKYEMU_API uint32_t se_get_draw_challenge_indicators(void);

/* Draw progress indicators: 0 = off, 1 = on */
SKYEMU_API void se_set_draw_progress_indicators(uint32_t value);
SKYEMU_API uint32_t se_get_draw_progress_indicators(void);

/* Draw leaderboard trackers: 0 = off, 1 = on */
SKYEMU_API void se_set_draw_leaderboard_trackers(uint32_t value);
SKYEMU_API uint32_t se_get_draw_leaderboard_trackers(void);

/* Draw notifications: 0 = off, 1 = on */
SKYEMU_API void se_set_draw_notifications(uint32_t value);
SKYEMU_API uint32_t se_get_draw_notifications(void);

/* GUI scale factor */
SKYEMU_API void se_set_gui_scale_factor(float scale);
SKYEMU_API float se_get_gui_scale_factor(void);

/* Only one notification at a time: 0 = off, 1 = on */
SKYEMU_API void se_set_only_one_notification(uint32_t value);
SKYEMU_API uint32_t se_get_only_one_notification(void);

/* Enable download cache: 0 = off, 1 = on */
SKYEMU_API void se_set_enable_download_cache(uint32_t value);
SKYEMU_API uint32_t se_get_enable_download_cache(void);

/* NDS layout index */
SKYEMU_API void se_set_nds_layout(uint32_t layout);
SKYEMU_API uint32_t se_get_nds_layout(void);

/* Touch screen show button labels: 0 = off, 1 = on */
SKYEMU_API void se_set_touch_screen_show_button_labels(uint32_t value);
SKYEMU_API uint32_t se_get_touch_screen_show_button_labels(void);

/* Show screen bezel: 0 = off, 1 = on */
SKYEMU_API void se_set_show_screen_bezel(uint32_t value);
SKYEMU_API uint32_t se_get_show_screen_bezel(void);

/* Draw debug menu: 0 = off, 1 = on */
SKYEMU_API void se_set_draw_debug_menu(uint32_t value);
SKYEMU_API uint32_t se_get_draw_debug_menu(void);

/*
   * New API: send a key event directly to the emulator.
   * The function is exported on all platforms. It can be called
   * from native code or via JNI on Android.
   */
SKYEMU_API void se_send_key(const char* key, float value);

/*
 * SkyEmu Framebuffer Interface
 * 
 * The framebuffer is RGBA: 4 bytes per pixel in red, green, blue, alpha order.
 */

/* System types matching SkyEmu's internal definitions */
#define SE_SYSTEM_NONE  0
#define SE_SYSTEM_GB    1
#define SE_SYSTEM_GBA   2
#define SE_SYSTEM_NDS   3

/* Screen dimensions */
#define SE_GBA_LCD_W    240
#define SE_GBA_LCD_H    160

#define SE_NDS_LCD_W    256
#define SE_NDS_LCD_H    192

#define SE_GB_LCD_W     160
#define SE_GB_LCD_H     144

/* Maximum framebuffer size (NDS top + bottom) */
#define SE_MAX_FRAMEBUFFER_SIZE (SE_NDS_LCD_W * SE_NDS_LCD_H * 4 * 2)

/* Get the currently emulated system */
SKYEMU_API int se_get_system(void);

/* Get the framebuffer dimensions */
SKYEMU_API void se_get_framebuffer_dimensions(int* width, int* height);

/* Get the number of framebuffers for the current system */
SKYEMU_API int se_get_framebuffer_count(void);

/* Get a pointer to the framebuffer data (RGBA format) */
SKYEMU_API const uint8_t* se_get_framebuffer(int screen_index);

/* Copy the framebuffer to a caller-provided buffer */
SKYEMU_API int se_copy_framebuffer(uint8_t* buffer, int buffer_size);

/* Check if a frame is ready */
SKYEMU_API bool se_is_frame_ready(void);

/* Get the framebuffer as a contiguous buffer */
SKYEMU_API const uint8_t* se_get_screenshot(int* out_width, int* out_height);

#ifdef SE_PLATFORM_WINDOWS_DLL
SKYEMU_API int win_main(int argc, char* argv[]);

    // 2. Export a function that takes the callback
SKYEMU_API void set_remote_keycode_callback(RemoteKeycodeCallback callback);
SKYEMU_API void set_ping_callback(PingCallback callback);
SKYEMU_API void set_external_menu_callback(ExternalMenuCallback callback);
SKYEMU_API void se_capture_state_slot(int slot);
SKYEMU_API void se_restore_state_slot(int slot);
#endif

#ifdef __cplusplus
}
#endif

#endif /* SKYEMU_DLL_H */