#include "se_settings.h"
#include "se_design.h"
#include "localization.h"
#include <math.h>
#include <string.h>

static float bounded(float value, float lo, float hi, float fallback){
  if(!isfinite(value))return fallback;
  return value<lo?lo:value>hi?hi:value;
}

void se_settings_validate(persistent_settings_t* s){
  s->volume = bounded(s->volume,0,1,0.8f);
  s->ghosting = bounded(s->ghosting,0,1,1);
  s->color_correction = bounded(s->color_correction,0,1,1);
  s->gui_scale_factor = bounded(s->gui_scale_factor,0.5f,4,1);
  s->custom_font_scale = bounded(s->custom_font_scale,0.5f,2,1);
  s->touch_controls_scale = bounded(s->touch_controls_scale,0.1f,3,1);
  s->touch_controls_opacity = bounded(s->touch_controls_opacity,0,1,0.5f);
  s->scanline_strength = bounded(s->scanline_strength,0,1,0.35f);
  s->mask_strength = bounded(s->mask_strength,0,1,0.25f);
  s->curvature = bounded(s->curvature,0,0.25f,0.08f);
  s->vignette = bounded(s->vignette,0,1,0.15f);
  s->display_brightness = bounded(s->display_brightness,0.5f,1.5f,1);
  s->display_saturation = bounded(s->display_saturation,0,2,1);
  s->display_contrast = bounded(s->display_contrast,0.5f,1.5f,1);
  s->corner_radius_scale = bounded(s->corner_radius_scale,0,2,1);
  if(s->screen_shader>=SE_SHADER_COUNT)s->screen_shader=SE_SHADER_SUBPIXELS;
  if(s->design_system>=SE_DESIGN_COUNT)s->design_system=SE_DESIGN_AUTO;
  if(s->color_scheme>=SE_COLOR_SCHEME_COUNT)s->color_scheme=SE_COLOR_SCHEME_SYSTEM;
  if(s->theme>3)s->theme=0;
  if(s->screen_rotation>3)s->screen_rotation=0;
  if(s->nds_layout>8)s->nds_layout=0;
  if(s->ui_density>2)s->ui_density=0;
  if(s->language>=SE_MAX_LANG_VALUE)s->language=SE_LANG_DEFAULT;
  if(s->gba_color_correction_mode>1)s->gba_color_correction_mode=0;
  if(s->avoid_overlaping_touchscreen>3)s->avoid_overlaping_touchscreen=0;
  if(s->http_control_server_port==0 || s->http_control_server_port>65535)s->http_control_server_port=8080;
  s->custom_accent &= 0xffffff;
  s->high_contrast = !!s->high_contrast;
}

void se_settings_migrate(persistent_settings_t* s, bool mobile){
  if(s->settings_file_version>SE_SETTINGS_VERSION){
    memset(s,0,sizeof(*s));
    s->volume=0.8f;
  }
  if(s->settings_file_version<1){
    static const uint32_t palette[]={0x388F81,0x437D64,0x3F6D56,0x2D4A31};
    memcpy(s->gb_palette,palette,sizeof(palette));
    s->ghosting=1; s->color_correction=1;
    s->screen_shader=SE_SHADER_SUBPIXELS;
    s->integer_scaling=0; s->screen_rotation=0; s->stretch_to_fit=0;
  }
  if(s->settings_file_version<2){
    s->auto_hide_touch_controls=1; s->touch_controls_opacity=0.5f;
    s->always_show_menubar=1; s->language=SE_LANG_DEFAULT;
    s->touch_controls_scale=1; s->touch_controls_show_turbo=1;
    s->save_to_path=0; s->http_control_server_enable=1;
    s->http_control_server_port=8080; s->avoid_overlaping_touchscreen=0;
  }
  if(s->settings_file_version<3){
    s->gui_scale_factor=1; s->custom_font_scale=1; s->hardcore_mode=0;
    s->draw_challenge_indicators=1; s->draw_progress_indicators=1;
    s->draw_leaderboard_trackers=1; s->draw_notifications=1;
    s->show_screen_bezel=1; s->only_one_notification=mobile;
    s->enable_download_cache=1; s->nds_layout=0; s->touch_screen_show_button_labels=1;
  }
  if(s->settings_file_version<4){
    s->design_system=s->theme==3?SE_DESIGN_CLASSIC:SE_DESIGN_AUTO;
    s->color_scheme=s->theme==1?SE_COLOR_SCHEME_LIGHT:s->theme==2?SE_COLOR_SCHEME_BLACK:SE_COLOR_SCHEME_SYSTEM;
    s->use_custom_accent=0; s->custom_accent=0; s->use_bundled_font=0;
  }
  if(s->settings_file_version<5){
    s->scanline_strength=0.35f; s->mask_strength=0.25f;
    s->curvature=0.08f; s->vignette=0.15f;
    s->display_brightness=1; s->display_saturation=1; s->display_contrast=1;
    s->high_contrast=0; s->ui_density=0; s->corner_radius_scale=1;
  }
  s->settings_file_version=SE_SETTINGS_VERSION;
  se_settings_validate(s);
}
