package com.sky.SkyEmu;

import android.app.Activity;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.View;
import android.widget.EditText;

import androidx.browser.customtabs.CustomTabsIntent;

import java.util.Locale;
import java.util.Vector;

public class MainSkyEmuObject {
    public void initialize(Bundle savedInstanceState, Activity activity) {
        NativeBridge.initialize(activity);
    }

    public native void se_android_load_file(String filePath);
    public native void se_android_load_rom(String filePath);
    public native void se_android_load_html(String filePath);
    public native void se_android_show_ui(boolean isShow);
    public native void se_android_stretch_on();
    public native void se_android_stretch_off();
    public native void se_android_capture_state_slot(int slot);
    public native void se_android_restore_state_slot(int slot);
    public native void se_android_send_key(String key, float value);
    public native void se_android_set_screen_shader(int shaderMode);
    public native int se_android_get_screen_shader();

    /* ---- Volume / Theme / Display ---- */
    public native void se_android_set_volume(float volume);
    public native float se_android_get_volume();
    public native void se_android_set_theme(int theme);
    public native int se_android_get_theme();
    public native void se_android_set_integer_scaling(int value);
    public native int se_android_get_integer_scaling();
    public native void se_android_set_screen_rotation(int rotation);
    public native int se_android_get_screen_rotation();
    public native void se_android_set_stretch_to_fit(int value);
    public native int se_android_get_stretch_to_fit();

    /* ---- Design system ----
     * design: 0 platform native (Material 3 on Android), 1 SkyEmu classic, 2 Material 3, 3 Fluent, 4 Adwaita
     * scheme: 0 follow system, 1 light, 2 dark, 3 black (AMOLED)
     * contrast: 0 follow system, 1 standard, 2 high
     * accent: 0xRRGGBB, or -1 to use the Material You wallpaper colors */
    public native void se_android_set_design_system(int design);
    public native int se_android_get_design_system();
    public native void se_android_set_color_scheme(int scheme);
    public native int se_android_get_color_scheme();
    public native void se_android_set_contrast(int contrast);
    public native int se_android_get_contrast();
    public native void se_android_set_accent_color(int rgb);
    public native int se_android_get_accent_color();
    /* For hosts that embed SkyEmu in their own activity: dark 1/0/-1, accent ARGB or -1 */
    public native void se_android_set_system_appearance(int dark, int accent);
    /* For hosts that embed SkyEmu in their own activity: system high contrast 1/0/-1 */
    public native void se_android_set_system_high_contrast(int highContrast);

    /* ---- GB Palette (index 0-3) ---- */
    public native void se_android_set_gb_palette(int index, int color);
    public native int se_android_get_gb_palette(int index);

    /* ---- Visual Effects ---- */
    public native void se_android_set_ghosting(float ghosting);
    public native float se_android_get_ghosting();
    public native void se_android_set_color_correction(float value);
    public native float se_android_get_color_correction();

    /* ---- Touch Controls ---- */
    public native void se_android_set_auto_hide_touch_controls(int value);
    public native int se_android_get_auto_hide_touch_controls();
    public native void se_android_set_touch_controls_opacity(float opacity);
    public native float se_android_get_touch_controls_opacity();
    public native void se_android_set_touch_controls_scale(float scale);
    public native float se_android_get_touch_controls_scale();
    public native void se_android_set_touch_controls_show_turbo(int value);
    public native int se_android_get_touch_controls_show_turbo();
    /* On-screen controller: 1 shown, 0 off */
    public native void se_android_set_touch_controller(int shown);
    public native int se_android_get_touch_controller();
    /* Rewind and Fast Forward buttons on the on-screen controller */
    public native void se_android_set_touch_controls_show_speed(int value);
    public native int se_android_get_touch_controls_show_speed();
    /* Restores the default on-screen controller layouts */
    public native void se_android_reset_touch_layout();
    /* Game controller face buttons: 0 match the labels, 1 match the GBA positions (A on the right) */
    public native void se_android_set_controller_face_layout(int layout);
    /* ROM patches (IPS, UPS, BPS): adds a patch to the loaded game and reloads it */
    public native boolean se_android_load_patch(String path);
    public native String se_android_get_patch_status();
    public native void se_android_set_soft_patching(int enabled);
    public native int se_android_get_soft_patching();
    /* Cheats of the loaded game, see skyemu_dll.h. Addresses and values are unsigned 32 bit numbers. */
    public native int se_android_add_cheat(String name, String code, int enabled);
    public native boolean se_android_remove_cheat(int index);
    public native boolean se_android_set_cheat_enabled(int index, int enabled);
    public native String se_android_get_cheats_json();
    /* Cheat finder: compare is 0 equal, 1 not equal, 2 greater, 3 less, 4 changed, 5 unchanged,
       6 increased, 7 decreased, 8 increased by, 9 decreased by */
    public native boolean se_android_cheat_search_start(int valueSize, int signed);
    public native long se_android_cheat_search_filter(int compare, long value);
    public native void se_android_cheat_search_reset();
    public native String se_android_get_cheat_search_json(int first, int max);
    public native int se_android_cheat_search_add_code(long address, long value, String name);
    public native int se_android_make_cheat(long address, long value, int valueSize, String name);
    /* RetroAchievements: login state is 0 logged out, 1 logging in, 2 logged in */
    public native void se_android_ra_login(String username, String password);
    public native void se_android_ra_logout();
    public native int se_android_ra_get_login_state();
    public native String se_android_ra_get_login_error();
    public native void se_android_set_ra_unofficial(int enabled);
    public native int se_android_get_ra_unofficial();
    public native void se_android_set_ra_spectator(int enabled);
    public native int se_android_get_ra_spectator();
    public native String se_android_get_achievements_json();
    public native int se_android_get_controller_face_layout();
    public native void se_android_set_avoid_overlapping_touchscreen(int value);
    public native int se_android_get_avoid_overlapping_touchscreen();
    public native void se_android_set_touch_screen_show_button_labels(int value);
    public native int se_android_get_touch_screen_show_button_labels();

    /* ---- UI / Menubar ---- */
    public native void se_android_set_always_show_menubar(int value);
    public native int se_android_get_always_show_menubar();
    public native void se_android_set_gui_scale_factor(float scale);
    public native float se_android_get_gui_scale_factor();
    public native void se_android_set_custom_font_scale(float scale);
    public native float se_android_get_custom_font_scale();

    /* ---- Emulation Options ---- */
    public native void se_android_set_force_dmg_mode(int value);
    public native int se_android_get_force_dmg_mode();
    public native void se_android_set_gba_color_correction_mode(int mode);
    public native int se_android_get_gba_color_correction_mode();
    public native void se_android_set_save_to_path(int value);
    public native int se_android_get_save_to_path();
    public native void se_android_set_nds_layout(int layout);
    public native int se_android_get_nds_layout();
    public native void se_android_set_show_screen_bezel(int value);
    public native int se_android_get_show_screen_bezel();

    /* ---- Language ---- */
    public native void se_android_set_language(int language);
    public native int se_android_get_language();

    /* ---- HTTP Control Server ---- */
    public native void se_android_set_http_control_server_enable(int value);
    public native int se_android_get_http_control_server_enable();
    public native void se_android_set_http_control_server_port(int port);
    public native int se_android_get_http_control_server_port();

    /* ---- RetroAchievements ---- */
    public native void se_android_set_hardcore_mode(int value);
    public native int se_android_get_hardcore_mode();
    public native void se_android_set_draw_challenge_indicators(int value);
    public native int se_android_get_draw_challenge_indicators();
    public native void se_android_set_draw_progress_indicators(int value);
    public native int se_android_get_draw_progress_indicators();
    public native void se_android_set_draw_leaderboard_trackers(int value);
    public native int se_android_get_draw_leaderboard_trackers();
    public native void se_android_set_draw_notifications(int value);
    public native int se_android_get_draw_notifications();
    public native void se_android_set_only_one_notification(int value);
    public native int se_android_get_only_one_notification();

    /* ---- Misc ---- */
    public native void se_android_set_enable_download_cache(int value);
    public native int se_android_get_enable_download_cache();
    public native void se_android_set_draw_debug_menu(int value);
    public native int se_android_get_draw_debug_menu();
}