/*****************************************************************************
 *
 *   SkyEmu design systems
 *
 *   See se_design.h. Color math follows Material's HCT model in spirit:
 *   "tone" is CIELAB L* so contrast between tones is predictable, while hue
 *   and chroma are OKLCH (a close, much smaller stand-in for CAM16). Chroma
 *   is clipped to the sRGB gamut per tone, which is also what HCT does, so
 *   the Material 3 baseline palette is reproduced within a few units.
 *
**/
#include "se_design.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #ifdef _MSC_VER
  #pragma comment(lib, "advapi32.lib")
  #pragma comment(lib, "user32.lib")
  #endif
#endif

#if defined(SE_PLATFORM_LINUX) || defined(SE_PLATFORM_FREEBSD)
  #define SE_DESIGN_FREEDESKTOP 1
  #include <X11/Xlib.h>
#endif

#define SE_DESIGN_PI 3.14159265358979f

const float se_standard_tones[SE_NUM_STANDARD_TONES]={100,99,95,90,80,70,60,50,40,30,20,10,0};

/*** Color science ***/

static float se_srgb_to_linear(float c){return c<=0.04045f? c/12.92f : powf((c+0.055f)/1.055f,2.4f);}
static float se_linear_to_srgb(float c){return c<=0.0031308f? c*12.92f : 1.055f*powf(c,1.0f/2.4f)-0.055f;}

static void se_rgb_to_linear(uint32_t rgb, float out[3]){
  out[0]=se_srgb_to_linear(((rgb>>16)&0xff)/255.f);
  out[1]=se_srgb_to_linear(((rgb>>8)&0xff)/255.f);
  out[2]=se_srgb_to_linear((rgb&0xff)/255.f);
}
static uint32_t se_linear_to_rgb(const float in[3]){
  uint32_t rgb = 0;
  for(int i=0;i<3;++i){
    float c = se_linear_to_srgb(in[i]<0?0:in[i]>1?1:in[i]);
    rgb = (rgb<<8)|(uint32_t)(c*255.f+0.5f);
  }
  return rgb;
}
// CIELAB L* from relative luminance (D65 white Y=1)
static float se_tone_from_y(float y){
  if(y<=0)return 0;
  float f = y>216.f/24389.f? cbrtf(y) : (24389.f/27.f*y+16.f)/116.f;
  return 116.f*f-16.f;
}
static float se_y_from_linear(const float c[3]){return 0.2126729f*c[0]+0.7151522f*c[1]+0.0721750f*c[2];}

static void se_linear_to_oklab(const float c[3], float lab[3]){
  float l = cbrtf(0.4122214708f*c[0]+0.5363325363f*c[1]+0.0514459929f*c[2]);
  float m = cbrtf(0.2119034982f*c[0]+0.6806995451f*c[1]+0.1073969566f*c[2]);
  float s = cbrtf(0.0883024619f*c[0]+0.2817188376f*c[1]+0.6299787005f*c[2]);
  lab[0]=0.2104542553f*l+0.7936177850f*m-0.0040720468f*s;
  lab[1]=1.9779984951f*l-2.4285922050f*m+0.4505937099f*s;
  lab[2]=0.0259040371f*l+0.7827717662f*m-0.8086757660f*s;
}
static void se_oklab_to_linear(const float lab[3], float c[3]){
  float l = lab[0]+0.3963377774f*lab[1]+0.2158037573f*lab[2];
  float m = lab[0]-0.1055613458f*lab[1]-0.0638541728f*lab[2];
  float s = lab[0]-0.0894841775f*lab[1]-1.2914855480f*lab[2];
  l=l*l*l; m=m*m*m; s=s*s*s;
  c[0]= 4.0767416621f*l-3.3077115913f*m+0.2309699292f*s;
  c[1]=-1.2684380046f*l+2.6097574011f*m-0.3413193965f*s;
  c[2]=-0.0041960863f*l-0.7034186147f*m+1.7076147010f*s;
}
static void se_rgb_to_oklch(uint32_t rgb, float* L, float* C, float* h){
  float lin[3], lab[3];
  se_rgb_to_linear(rgb,lin);
  se_linear_to_oklab(lin,lab);
  *L = lab[0];
  *C = sqrtf(lab[1]*lab[1]+lab[2]*lab[2]);
  float hue = atan2f(lab[2],lab[1])*180.f/SE_DESIGN_PI;
  *h = hue<0? hue+360.f: hue;
}
float se_color_tone(uint32_t rgb){
  float lin[3];
  se_rgb_to_linear(rgb,lin);
  return se_tone_from_y(se_y_from_linear(lin));
}
float se_contrast_ratio(uint32_t a, uint32_t b){
  float la[3], lb[3];
  se_rgb_to_linear(a,la);
  se_rgb_to_linear(b,lb);
  float ya = se_y_from_linear(la)+0.05f, yb = se_y_from_linear(lb)+0.05f;
  return ya>yb? ya/yb : yb/ya;
}
// Solves the OKLab lightness that produces the requested tone for a fixed hue
// and chroma. Returns false when the resulting color is outside of sRGB.
static bool se_solve_tone(float hue, float chroma, float tone, float lin[3]){
  float lab[3]={0, chroma*cosf(hue*SE_DESIGN_PI/180.f), chroma*sinf(hue*SE_DESIGN_PI/180.f)};
  float lo = 0, hi = 1;
  for(int i=0;i<32;++i){
    lab[0]=(lo+hi)*0.5f;
    se_oklab_to_linear(lab,lin);
    if(se_tone_from_y(se_y_from_linear(lin))<tone)lo=lab[0];
    else hi=lab[0];
  }
  lab[0]=(lo+hi)*0.5f;
  se_oklab_to_linear(lab,lin);
  const float eps = 1e-4f;
  for(int i=0;i<3;++i)if(lin[i]<-eps||lin[i]>1.f+eps)return false;
  return true;
}
static uint32_t se_hue_chroma_tone(float hue, float chroma, float tone){
  if(tone<=0.f)return 0x000000;
  if(tone>=100.f)return 0xffffff;
  float lin[3];
  if(chroma<=0.f){se_solve_tone(hue,0,tone,lin); return se_linear_to_rgb(lin);}
  if(se_solve_tone(hue,chroma,tone,lin))return se_linear_to_rgb(lin);
  // Reduce chroma until the color fits in sRGB, keeping hue and tone exact.
  float lo = 0, hi = chroma;
  for(int i=0;i<20;++i){
    float mid = (lo+hi)*0.5f;
    if(se_solve_tone(hue,mid,tone,lin))lo=mid;
    else hi=mid;
  }
  se_solve_tone(hue,lo,tone,lin);
  return se_linear_to_rgb(lin);
}

/*** Tonal palettes ***/

void se_tonal_palette_from_hue_chroma(float hue, float chroma, se_tonal_palette_t* out){
  memset(out,0,sizeof(*out));
  out->hue = fmodf(hue+360.f,360.f);
  out->chroma = chroma;
}
void se_tonal_palette_from_color(uint32_t rgb, se_tonal_palette_t* out){
  float L,C,h;
  se_rgb_to_oklch(rgb,&L,&C,&h);
  se_tonal_palette_from_hue_chroma(h,C,out);
}
void se_tonal_palette_from_tones(const uint32_t rgb_at_standard_tones[SE_NUM_STANDARD_TONES], se_tonal_palette_t* out){
  // Hue from tone 40 (index 8) where palettes are most colorful, chroma is the
  // strongest of the mid tones so generated in-between tones are not dull.
  float L,C,h;
  se_rgb_to_oklch(rgb_at_standard_tones[8],&L,&C,&h);
  float max_c = C;
  for(int i=6;i<=10;++i){
    float l2,c2,h2;
    se_rgb_to_oklch(rgb_at_standard_tones[i],&l2,&c2,&h2);
    if(c2>max_c)max_c=c2;
  }
  se_tonal_palette_from_hue_chroma(h,max_c,out);
  out->has_exact = true;
  for(int i=0;i<SE_NUM_STANDARD_TONES;++i)out->exact[i]=rgb_at_standard_tones[i]&0xffffff;
}
uint32_t se_tonal_palette_tone(const se_tonal_palette_t* palette, float tone){
  if(palette->has_exact){
    for(int i=0;i<SE_NUM_STANDARD_TONES;++i){
      if(fabsf(se_standard_tones[i]-tone)<0.01f)return palette->exact[i];
    }
  }
  return se_hue_chroma_tone(palette->hue,palette->chroma,tone);
}
void se_core_palette_from_seed(uint32_t seed, se_core_palette_t* out){
  // Material 3 "tonal spot": a restrained primary, desaturated secondary, a
  // tertiary rotated 60 degrees and near neutral surfaces tinted by the seed.
  // Chroma values are the OKLCH equivalents of Material's CAM16 chroma
  // (36, 16, 24, 6, 8), calibrated against the baseline #6750A4 scheme.
  float L,C,h;
  se_rgb_to_oklch(seed,&L,&C,&h);
  float primary_chroma = C<0.10f? 0.10f : C>0.14f? 0.14f : C;
  se_tonal_palette_from_hue_chroma(h,primary_chroma,&out->primary);
  se_tonal_palette_from_hue_chroma(h,0.036f,&out->secondary);
  se_tonal_palette_from_hue_chroma(h+60.f,0.062f,&out->tertiary);
  se_tonal_palette_from_hue_chroma(h,0.011f,&out->neutral);
  se_tonal_palette_from_hue_chroma(h,0.020f,&out->neutral_variant);
}

/*** Color helpers ***/

se_color_t se_color_from_rgb(uint32_t rgb, float alpha){
  se_color_t c = {((rgb>>16)&0xff)/255.f,((rgb>>8)&0xff)/255.f,(rgb&0xff)/255.f,alpha};
  return c;
}
uint32_t se_color_to_rgb(se_color_t c){
  uint32_t r = (uint32_t)(fminf(fmaxf(c.r,0),1)*255.f+0.5f);
  uint32_t g = (uint32_t)(fminf(fmaxf(c.g,0),1)*255.f+0.5f);
  uint32_t b = (uint32_t)(fminf(fmaxf(c.b,0),1)*255.f+0.5f);
  return (r<<16)|(g<<8)|b;
}
se_color_t se_color_blend(se_color_t base, se_color_t over){
  se_color_t out;
  out.a = over.a+base.a*(1.f-over.a);
  if(out.a<=0.f){out.r=out.g=out.b=0;return out;}
  out.r = (over.r*over.a+base.r*base.a*(1.f-over.a))/out.a;
  out.g = (over.g*over.a+base.g*base.a*(1.f-over.a))/out.a;
  out.b = (over.b*over.a+base.b*base.a*(1.f-over.a))/out.a;
  return out;
}
static se_color_t se_rgba(uint32_t rgb, float a){return se_color_from_rgb(rgb,a);}
static se_color_t se_tone(const se_tonal_palette_t* p, float tone){return se_color_from_rgb(se_tonal_palette_tone(p,tone),1.f);}
static se_color_t se_alpha(se_color_t c, float a){c.a = a; return c;}
// Accent with OKLab lightness clamped to [min_l,max_l] (libadwaita's accent_color rule)
static se_color_t se_accent_with_lightness(uint32_t rgb, float min_l, float max_l){
  float lin[3], lab[3];
  se_rgb_to_linear(rgb,lin);
  se_linear_to_oklab(lin,lab);
  if(lab[0]<min_l)lab[0]=min_l;
  if(lab[0]>max_l)lab[0]=max_l;
  se_oklab_to_linear(lab,lin);
  return se_color_from_rgb(se_linear_to_rgb(lin),1.f);
}

/*** Design selection ***/

int se_design_platform_default(void){
#if defined(SE_PLATFORM_ANDROID)
  return SE_DESIGN_MATERIAL3;
#elif defined(_WIN32)
  return SE_DESIGN_FLUENT;
#elif defined(SE_PLATFORM_LINUX) || defined(SE_PLATFORM_FREEBSD)
  return SE_DESIGN_ADWAITA;
#elif defined(EMSCRIPTEN) || defined(SE_PLATFORM_WEB)
  return SE_DESIGN_MATERIAL3;
#else
  // Apple platforms keep the original SkyEmu look
  return SE_DESIGN_CLASSIC;
#endif
}
int se_design_resolve(int design){
  if(design<=SE_DESIGN_AUTO||design>=SE_DESIGN_COUNT)return se_design_platform_default();
  return design;
}
const char* se_design_name(int design){
  switch(design){
    case SE_DESIGN_AUTO:      return "Platform Native";
    case SE_DESIGN_CLASSIC:   return "SkyEmu Classic";
    case SE_DESIGN_MATERIAL3: return "Material 3";
    case SE_DESIGN_FLUENT:    return "Fluent (Windows 11)";
    case SE_DESIGN_ADWAITA:   return "Adwaita (GNOME)";
  }
  return "Unknown";
}
bool se_design_is_dark(int color_scheme, const se_system_appearance_t* sys){
  switch(color_scheme){
    case SE_COLOR_SCHEME_LIGHT: return false;
    case SE_COLOR_SCHEME_DARK:
    case SE_COLOR_SCHEME_BLACK: return true;
  }
  // SkyEmu has always defaulted to dark when the OS does not say otherwise
  if(sys&&sys->dark>=0)return sys->dark!=0;
  return true;
}
bool se_design_is_high_contrast(int contrast, const se_system_appearance_t* sys){
  switch(contrast){
    case SE_CONTRAST_STANDARD: return false;
    case SE_CONTRAST_HIGH: return true;
  }
  return sys&&sys->high_contrast>0;
}
void se_design_default_contrast_colors(bool dark, se_contrast_colors_t* out){
  static const se_contrast_colors_t night_sky = {0x000000,0xFFFFFF,0xD6B4FD,0x2B2B2B,0x8080FF,0xA6A6A6,0x000000,0xFFFFFF};
  static const se_contrast_colors_t desert    = {0xFFFAEF,0x3D3D3D,0x903909,0xFFF5E3,0x1C5E75,0x676767,0xFFFAEF,0x202020};
  *out = dark? night_sky : desert;
}
uint32_t se_design_default_accent(int design){
  switch(design){
    case SE_DESIGN_MATERIAL3: return 0x6750A4; // Material 3 baseline seed
    case SE_DESIGN_FLUENT:    return 0x0078D4; // Windows default blue
    case SE_DESIGN_ADWAITA:   return 0x3584E4; // GNOME blue
  }
  return 0x6750A4;
}

/*** Material Design 3 ***/

static void se_build_material3(const se_core_palette_t* p, se_design_tokens_t* t){
  const se_tonal_palette_t *P=&p->primary, *S=&p->secondary, *T=&p->tertiary, *N=&p->neutral, *NV=&p->neutral_variant;
  bool dark = t->dark;
  bool hc = t->high_contrast;
  // High contrast follows Material's contrast level 1: accents move away from the surface,
  // accent containers become strong fills carrying white (light) or black (dark) text, and
  // text and outlines get the tones that reach Material's high contrast targets (11:1, 7:1).
  float container = dark? (hc? 80:30) : (hc? 30:90);
  float on_container = dark? (hc? 0:90) : (hc? 100:10);
  // Buttons, tabs and selected list items are filled with the secondary container but Dear
  // ImGui draws their labels with on_surface, so it keeps a tone that works with that text.
  // In high contrast these controls get an outline instead.
  float secondary = dark? (hc? 25:30) : (hc? 85:90);
  t->primary              = se_tone(P, dark? (hc? 90:80) : (hc? 30:40));
  t->on_primary           = se_tone(P, dark? (hc? 10:20) : 100);
  t->primary_container    = se_tone(P, container);
  t->on_primary_container = se_tone(P, on_container);
  t->secondary_container    = se_tone(S, secondary);
  t->on_secondary_container = se_tone(S, dark? (hc? 100:90) : (hc? 0:10));
  t->tertiary_container     = se_tone(T, container);
  t->on_tertiary_container  = se_tone(T, on_container);
  t->error    = se_rgba(dark? 0xF2B8B5:0xB3261E,1);
  t->on_error = se_rgba(dark? 0x601410:0xFFFFFF,1);
  t->accent_text = t->primary;
  if(!dark){
    t->background    = se_tone(N,98); // surface
    t->surface_panel = se_tone(N,96); // surfaceContainerLow (navigation drawer)
    t->surface_bar   = se_tone(N,94); // surfaceContainer (scrolled top app bar)
    t->surface_popup = se_tone(N,94); // surfaceContainer (menus)
    t->surface_card  = se_tone(N,92); // surfaceContainerHigh
    t->surface_input = se_tone(N,90); // surfaceContainerHighest (filled text fields)
  }else if(!t->black){
    t->background    = se_tone(N,6);
    t->surface_panel = se_tone(N,10);
    t->surface_bar   = se_tone(N,12);
    t->surface_popup = se_tone(N,17);
    t->surface_card  = se_tone(N,17);
    t->surface_input = se_tone(N,22);
  }else{
    t->background    = se_rgba(0x000000,1);
    t->surface_panel = se_tone(N,4);
    t->surface_bar   = se_tone(N,4);
    t->surface_popup = se_tone(N,12);
    t->surface_card  = se_tone(N,10);
    t->surface_input = se_tone(N,17);
  }
  t->surface_content     = t->background;
  t->on_surface          = se_tone(N, dark? (hc? 100:90) : (hc? 0:10));
  t->on_surface_variant  = se_tone(NV,dark? (hc? 90:80) : (hc? 20:30));
  t->on_surface_disabled = se_alpha(t->on_surface,hc? 0.6f:0.38f);
  t->outline             = se_tone(NV,dark? (hc? 70:60) : (hc? 35:50));
  t->outline_variant     = se_tone(NV,dark? (hc? 55:30) : (hc? 45:80));
  // Filled tonal buttons
  t->control        = t->secondary_container;
  t->control_hover  = se_color_blend(t->control,se_alpha(t->on_secondary_container,0.08f));
  t->control_active = se_color_blend(t->control,se_alpha(t->on_secondary_container,0.12f));
  t->selected    = t->primary;
  t->on_selected = t->on_primary;
  t->state_hover = se_alpha(t->on_surface,0.08f);
  t->state_press = se_alpha(t->on_surface,0.12f);
  t->scrim = se_rgba(0x000000,0.32f);

  t->panel_rounding = 16;  t->card_rounding = 12;   t->button_rounding = 100;
  t->input_rounding = 4;   t->check_rounding = 2;   t->popup_rounding = 4;
  t->scrollbar_rounding = 100;
  t->frame_padding_x = 12; t->frame_padding_y = 6;
  t->item_spacing_x = 8;   t->item_spacing_y = 8;
  t->window_padding = 12;
  t->scrollbar_size = 8;   t->grab_min_size = 12;
  t->control_border = hc? 1:0; t->popup_border = hc? 1:0; t->check_border = 2;
  t->font_size = 14;
  t->slider_track_h = 4;   t->slider_thumb_r = 10;
  t->section_accent = true;
  t->section_bold = true;
  t->section_divider = true;
  t->bar_divider = false;
}

/*** Fluent 2 / WinUI 3 (Windows 11) ***/

static void se_build_fluent(uint32_t accent, se_design_tokens_t* t){
  bool dark = t->dark;
  se_tonal_palette_t A;
  se_tonal_palette_from_color(accent,&A);
  // AccentFillColorDefault is SystemAccentColorLight2 in dark mode and
  // SystemAccentColorDark1 in light mode. The exact shades of the default
  // Windows blue are known, other accents use the matching tones.
  if(accent==0x0078D4){
    t->primary = se_rgba(dark? 0x60CDFF:0x005FB8,1);
    t->accent_text = se_rgba(dark? 0x99EBFF:0x003E92,1);
  }else{
    t->primary = se_tone(&A,dark? 78:40);
    t->accent_text = se_tone(&A,dark? 85:32);
  }
  t->on_primary = se_rgba(dark? 0x000000:0xFFFFFF,1);
  if(dark){
    // Title bar and navigation pane sit on Mica, content pages on a layer above it
    t->background      = se_rgba(t->black? 0x000000:0x202020,1); // Mica base
    t->surface_bar     = t->background;
    t->surface_panel   = t->background;
    t->surface_content = se_rgba(t->black? 0x0C0C0C:0x282828,1); // Layer over Mica
    t->surface_card  = se_rgba(0xFFFFFF,0.0512f);
    t->surface_popup = se_rgba(t->black? 0x1C1C1C:0x2C2C2C,1); // Acrylic flyout
    t->surface_input = se_rgba(0xFFFFFF,0.0605f);
    t->on_surface          = se_rgba(0xFFFFFF,1);
    t->on_surface_variant  = se_rgba(0xFFFFFF,0.786f);
    t->on_surface_disabled = se_rgba(0xFFFFFF,0.3628f);
    t->outline         = se_rgba(0xFFFFFF,0.5442f); // ControlStrongStroke
    t->outline_variant = se_rgba(0xFFFFFF,0.0837f); // Divider stroke
    t->control         = se_rgba(0xFFFFFF,0.0605f);
    t->control_hover   = se_rgba(0xFFFFFF,0.0837f);
    t->control_active  = se_rgba(0xFFFFFF,0.0326f);
    t->secondary_container = se_rgba(0xFFFFFF,0.0605f); // SubtleFillSecondary
    t->state_hover = se_rgba(0xFFFFFF,0.0605f);
    t->state_press = se_rgba(0xFFFFFF,0.0419f);
    t->error = se_rgba(0xFF99A4,1);
    t->on_error = se_rgba(0x000000,1);
  }else{
    t->background      = se_rgba(0xF3F3F3,1);
    t->surface_bar     = t->background;
    t->surface_panel   = t->background;
    t->surface_content = se_rgba(0xF9F9F9,1);
    t->surface_card  = se_rgba(0xFFFFFF,0.7f);
    t->surface_popup = se_rgba(0xFCFCFC,1);
    t->surface_input = se_rgba(0xFFFFFF,0.7f);
    t->on_surface          = se_rgba(0x000000,0.8956f);
    t->on_surface_variant  = se_rgba(0x000000,0.6063f);
    t->on_surface_disabled = se_rgba(0x000000,0.3614f);
    t->outline         = se_rgba(0x000000,0.4458f);
    t->outline_variant = se_rgba(0x000000,0.0803f);
    t->control         = se_rgba(0xFFFFFF,0.7f);
    t->control_hover   = se_rgba(0xF9F9F9,0.5f);
    t->control_active  = se_rgba(0xF9F9F9,0.3f);
    t->secondary_container = se_rgba(0x000000,0.0373f);
    t->state_hover = se_rgba(0x000000,0.0373f);
    t->state_press = se_rgba(0x000000,0.0241f);
    t->error = se_rgba(0xC42B1C,1);
    t->on_error = se_rgba(0xFFFFFF,1);
  }
  t->on_secondary_container = t->on_surface;
  t->primary_container = se_alpha(t->primary,dark? 0.22f:0.16f);
  t->on_primary_container = t->accent_text;
  t->tertiary_container = se_alpha(t->primary,0.45f);
  t->on_tertiary_container = t->on_surface;
  t->selected = t->primary;
  t->on_selected = t->on_primary;
  t->scrim = se_rgba(0x000000,0.3f);

  t->panel_rounding = 8;   t->card_rounding = 4;    t->button_rounding = 4;
  t->input_rounding = 4;   t->check_rounding = 4;   t->popup_rounding = 8;
  t->scrollbar_rounding = 100;
  t->frame_padding_x = 11; t->frame_padding_y = 5;
  t->item_spacing_x = 8;   t->item_spacing_y = 6;
  t->window_padding = 12;
  t->scrollbar_size = 8;   t->grab_min_size = 12;
  t->control_border = 1;   t->popup_border = 1;   t->check_border = 1;
  t->font_size = 14;
  t->slider_track_h = 4;   t->slider_thumb_r = 10;
  t->section_bold = true;
  t->bar_divider = true;
  t->thumb_ring = true;
}

// In a contrast theme Windows replaces the Fluent colors with the theme's system colors
static void se_build_fluent_contrast(const se_contrast_colors_t* c, se_design_tokens_t* t){
  se_color_t window = se_rgba(c->window,1), text = se_rgba(c->window_text,1);
  se_color_t highlight = se_rgba(c->highlight,1), highlight_text = se_rgba(c->highlight_text,1);
  t->background = t->surface_bar = t->surface_panel = t->surface_content = window;
  t->surface_card = t->surface_popup = t->surface_input = window;
  t->on_surface = t->on_surface_variant = text;
  t->on_surface_disabled = se_rgba(c->gray_text,1);
  t->outline = t->outline_variant = text;
  t->control = t->secondary_container = se_rgba(c->button_face,1);
  t->on_secondary_container = se_rgba(c->button_text,1);
  t->control_hover  = se_color_blend(t->control,se_alpha(text,0.16f));
  t->control_active = se_color_blend(t->control,se_alpha(text,0.28f));
  t->primary = t->selected = t->primary_container = t->tertiary_container = highlight;
  t->on_primary = t->on_selected = t->on_primary_container = t->on_tertiary_container = highlight_text;
  t->accent_text = se_rgba(c->hotlight,1);
  t->state_hover = se_alpha(text,0.16f);
  t->state_press = se_alpha(text,0.28f);
  t->scrim = se_rgba(0x000000,0.5f);
  t->control_border = 1;   t->popup_border = 2;   t->check_border = 1;
}

/*** libadwaita (GNOME) ***/

static void se_build_adwaita(uint32_t accent, se_design_tokens_t* t){
  bool dark = t->dark;
  // libadwaita's high contrast style keeps the colors but draws borders around buttons,
  // makes borders and dimmed labels much stronger and the text fully opaque.
  bool hc = t->high_contrast;
  // accent_bg_color is the accent itself, accent_color (text on surfaces) clamps
  // its OKLab lightness like libadwaita 1.6 does.
  t->primary = se_rgba(accent,1);
  t->on_primary = se_rgba(0xFFFFFF,1);
  t->accent_text = dark? se_accent_with_lightness(accent,hc? 0.9f:0.85f,1.f) : se_accent_with_lightness(accent,0.f,hc? 0.4f:0.5f);
  uint32_t fg = dark? 0xFFFFFF : 0x000006;
  float fg_a = dark||hc? 1.f : 0.8f; // window_fg_color
  if(dark){
    t->background    = se_rgba(t->black? 0x000000:0x222226,1); // window_bg
    t->surface_bar   = se_rgba(t->black? 0x121214:0x2E2E32,1); // headerbar_bg
    t->surface_panel = se_rgba(t->black? 0x0E0E10:0x2E2E32,1); // sidebar_bg
    t->surface_card  = se_rgba(0xFFFFFF,0.08f);
    t->surface_popup = se_rgba(t->black? 0x1E1E20:0x36363A,1);
    t->error = se_rgba(0xC01C28,1);
  }else{
    t->background    = se_rgba(0xFAFAFB,1);
    t->surface_bar   = se_rgba(0xFFFFFF,1);
    t->surface_panel = se_rgba(0xEBEBED,1);
    t->surface_card  = se_rgba(0xFFFFFF,1);
    t->surface_popup = se_rgba(0xFFFFFF,1);
    t->error = se_rgba(0xE01B24,1);
  }
  t->on_error = se_rgba(0xFFFFFF,1);
  t->surface_content = t->background;
  // Widgets are tinted with alpha(currentColor, x) in libadwaita
  t->surface_input       = se_rgba(fg,(hc? 0.15f:0.1f)*fg_a);
  t->on_surface          = se_rgba(fg,fg_a);
  t->on_surface_variant  = se_rgba(fg,(hc? 0.9f:0.55f)*fg_a); // .dim-label
  t->on_surface_disabled = se_rgba(fg,0.5f*fg_a);
  t->outline             = se_rgba(fg,(hc? 0.6f:0.3f)*fg_a);
  t->outline_variant     = se_rgba(fg,(hc? 0.5f:0.15f)*fg_a);
  t->control             = se_rgba(fg,0.1f*fg_a);
  t->control_hover       = se_rgba(fg,0.15f*fg_a);
  t->control_active      = se_rgba(fg,0.3f*fg_a);
  t->secondary_container = se_rgba(fg,0.1f*fg_a);
  t->on_secondary_container = t->on_surface;
  t->primary_container   = se_alpha(t->primary,dark? 0.25f:0.15f);
  t->on_primary_container = t->accent_text;
  t->tertiary_container  = se_alpha(t->primary,0.45f);
  t->on_tertiary_container = t->on_surface;
  // Checked toggle buttons get a darker neutral, not the accent
  t->selected    = t->control_active;
  t->on_selected = t->on_surface;
  t->state_hover = se_rgba(fg,0.07f*fg_a);
  t->state_press = se_rgba(fg,0.16f*fg_a);
  t->scrim = se_rgba(0x000000,dark? 0.5f:0.3f);

  t->panel_rounding = 12;  t->card_rounding = 12;   t->button_rounding = 6;
  t->input_rounding = 6;   t->check_rounding = 4;   t->popup_rounding = 12;
  t->scrollbar_rounding = 100;
  t->frame_padding_x = 10; t->frame_padding_y = 5;
  t->item_spacing_x = 6;   t->item_spacing_y = 6;
  t->window_padding = 12;
  t->scrollbar_size = 8;   t->grab_min_size = 12;
  t->control_border = hc? 1:0; t->popup_border = 1; t->check_border = 2;
  t->font_size = 14;
  t->slider_track_h = 6;   t->slider_thumb_r = 10;
  t->section_bold = true;
  t->bar_divider = true;
  t->thumb_light = true;
}

void se_design_build_tokens(int design, int color_scheme, int contrast, uint32_t custom_accent,
                            const se_system_appearance_t* sys, se_design_tokens_t* t){
  memset(t,0,sizeof(*t));
  design = se_design_resolve(design);
  if(design==SE_DESIGN_CLASSIC)design = SE_DESIGN_MATERIAL3;
  t->design = design;
  t->dark = se_design_is_dark(color_scheme,sys);
  t->black = t->dark && color_scheme==SE_COLOR_SCHEME_BLACK;
  t->high_contrast = se_design_is_high_contrast(contrast,sys);
  uint32_t accent = custom_accent;
  if(accent==SE_ACCENT_NONE&&sys)accent = sys->accent;
  if(accent==SE_ACCENT_NONE)accent = se_design_default_accent(design);
  accent&=0xffffff;
  t->accent = accent;
  switch(design){
    case SE_DESIGN_FLUENT:{
      se_contrast_colors_t c = {0};
      if(t->high_contrast){
        // An active Windows contrast theme decides between light and dark itself
        if(sys&&sys->has_contrast_colors){
          c = sys->contrast_colors;
          t->dark = se_color_tone(c.window)<50.f;
          t->black = (c.window&0xffffff)==0;
        }else se_design_default_contrast_colors(t->dark,&c);
      }
      se_build_fluent(accent,t);
      if(t->high_contrast)se_build_fluent_contrast(&c,t);
    }break;
    case SE_DESIGN_ADWAITA: se_build_adwaita(accent,t); break;
    default:{
      se_core_palette_t core;
      // Use the wallpaper derived Material You palette unless the user picked a color
      if(custom_accent==SE_ACCENT_NONE&&sys&&sys->has_core_palette)core = sys->core_palette;
      else se_core_palette_from_seed(accent,&core);
      se_build_material3(&core,t);
    }break;
  }
}

/*** Platform integration ***/

#if defined(SE_DESIGN_FREEDESKTOP)
static uint32_t se_gnome_accent_from_name(const char* name){
  // GNOME 47 accent colors (libadwaita accent_bg_color values)
  static const struct{const char* name; uint32_t rgb;} accents[]={
    {"blue",0x3584E4},{"teal",0x2190A4},{"green",0x3A944A},{"yellow",0xC88800},{"orange",0xED5B00},
    {"red",0xE62D42},{"pink",0xD56199},{"purple",0x9141AC},{"slate",0x6F8396},
  };
  for(size_t i=0;i<sizeof(accents)/sizeof(accents[0]);++i){
    if(strstr(name,accents[i].name))return accents[i].rgb;
  }
  return SE_ACCENT_NONE;
}
#endif

void se_design_query_system_appearance(se_system_appearance_t* out){
  memset(out,0,sizeof(*out));
  out->dark = -1;
  out->accent = SE_ACCENT_NONE;
  out->high_contrast = -1;
#if defined(_WIN32)
  HIGHCONTRASTW hc;
  memset(&hc,0,sizeof(hc));
  hc.cbSize = sizeof(hc);
  if(SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(hc),&hc,0)){
    out->high_contrast = (hc.dwFlags&HCF_HIGHCONTRASTON)!=0;
    if(out->high_contrast){
      // The colors of the active contrast theme (Aquatic, Desert, Dusk, Night sky or custom)
      se_contrast_colors_t* c = &out->contrast_colors;
      const int ids[8]={COLOR_WINDOW,COLOR_WINDOWTEXT,COLOR_HIGHLIGHT,COLOR_HIGHLIGHTTEXT,
                        COLOR_HOTLIGHT,COLOR_GRAYTEXT,COLOR_BTNFACE,COLOR_BTNTEXT};
      uint32_t* dst[8]={&c->window,&c->window_text,&c->highlight,&c->highlight_text,
                        &c->hotlight,&c->gray_text,&c->button_face,&c->button_text};
      for(int i=0;i<8;++i){
        DWORD v = GetSysColor(ids[i]);
        *dst[i] = ((uint32_t)GetRValue(v)<<16)|((uint32_t)GetGValue(v)<<8)|GetBValue(v);
      }
      out->has_contrast_colors = true;
    }
  }
  DWORD value = 0, type = 0, size = sizeof(value);
  HKEY key;
  if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",0,KEY_READ,&key)==ERROR_SUCCESS){
    if(RegQueryValueExW(key,L"AppsUseLightTheme",NULL,&type,(LPBYTE)&value,&size)==ERROR_SUCCESS&&type==REG_DWORD){
      out->dark = value==0;
    }
    RegCloseKey(key);
  }
  size = sizeof(value);
  if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\DWM",0,KEY_READ,&key)==ERROR_SUCCESS){
    // AccentColor is stored as 0xAABBGGRR
    if(RegQueryValueExW(key,L"AccentColor",NULL,&type,(LPBYTE)&value,&size)==ERROR_SUCCESS&&type==REG_DWORD){
      out->accent = ((value&0xff)<<16)|(value&0xff00)|((value>>16)&0xff);
    }
    RegCloseKey(key);
  }
#elif defined(SE_DESIGN_FREEDESKTOP)
  // Ask the XDG desktop portal first (works under Flatpak and on KDE), then GNOME's settings.
  const char* script =
    "p(){ gdbus call --session --timeout 1 --dest org.freedesktop.portal.Desktop "
    "--object-path /org/freedesktop/portal/desktop --method org.freedesktop.portal.Settings.ReadOne "
    "org.freedesktop.appearance \"$1\" 2>/dev/null; };"
    "g(){ gsettings get org.gnome.desktop.interface \"$1\" 2>/dev/null; };"
    "echo \"portal-scheme=$(p color-scheme)\";"
    "echo \"portal-accent=$(p accent-color)\";"
    "echo \"gnome-scheme=$(g color-scheme)\";"
    "echo \"gnome-accent=$(g accent-color)\";"
    "echo \"gtk-theme=$(g gtk-theme)\";"
    "echo \"portal-contrast=$(p contrast)\";"
    "echo \"gnome-hc=$(gsettings get org.gnome.desktop.a11y.interface high-contrast 2>/dev/null)\"";
  char cmd[1024];
  snprintf(cmd,sizeof(cmd),"sh -c '%s' 2>/dev/null",script);
  FILE* f = popen(cmd,"r");
  if(f){
    char line[512];
    int portal_scheme = -1, gnome_dark = -1, gtk_dark = -1, portal_contrast = -1, gnome_hc = -1;
    uint32_t portal_accent = SE_ACCENT_NONE, gnome_accent = SE_ACCENT_NONE;
    while(fgets(line,sizeof(line),f)){
      char* v = strchr(line,'=');
      if(!v)continue;
      *v++ = 0;
      if(strcmp(line,"portal-scheme")==0){
        // "(<uint32 1>,)": 0 no preference, 1 prefer dark, 2 prefer light
        const char* n = strstr(v,"uint32 ");
        if(n)portal_scheme = atoi(n+7);
      }else if(strcmp(line,"portal-accent")==0){
        // "(<(0.207, 0.517, 0.894)>,)", components outside [0,1] mean unset
        const char* n = strstr(v,"<(");
        double r,g,b;
        if(n&&sscanf(n+2,"%lf, %lf, %lf",&r,&g,&b)==3&&r>=0&&r<=1&&g>=0&&g<=1&&b>=0&&b<=1){
          portal_accent = ((uint32_t)(r*255+0.5)<<16)|((uint32_t)(g*255+0.5)<<8)|(uint32_t)(b*255+0.5);
        }
      }else if(strcmp(line,"gnome-scheme")==0){
        if(strstr(v,"prefer-dark"))gnome_dark = 1;
        else if(strstr(v,"default")||strstr(v,"prefer-light"))gnome_dark = 0;
      }else if(strcmp(line,"gnome-accent")==0){
        gnome_accent = se_gnome_accent_from_name(v);
      }else if(strcmp(line,"gtk-theme")==0){
        if(strstr(v,"'"))gtk_dark = strstr(v,"dark")||strstr(v,"Dark");
      }else if(strcmp(line,"portal-contrast")==0){
        // "(<uint32 1>,)": 0 no preference, 1 higher contrast
        const char* n = strstr(v,"uint32 ");
        if(n)portal_contrast = atoi(n+7);
      }else if(strcmp(line,"gnome-hc")==0){
        if(strstr(v,"true"))gnome_hc = 1;
        else if(strstr(v,"false"))gnome_hc = 0;
      }
    }
    pclose(f);
    if(portal_scheme==1)out->dark = 1;
    else if(portal_scheme==2)out->dark = 0;
    else if(gnome_dark>=0)out->dark = gnome_dark;
    else if(portal_scheme==0)out->dark = 0;
    else if(gtk_dark>=0)out->dark = gtk_dark;
    out->accent = portal_accent!=SE_ACCENT_NONE? portal_accent : gnome_accent;
    // High contrast when either source asks for it
    if(portal_contrast==1||gnome_hc==1)out->high_contrast = 1;
    else if(portal_contrast==0||gnome_hc==0)out->high_contrast = 0;
  }
  const char* gtk_theme = getenv("GTK_THEME");
  if(out->dark<0&&gtk_theme)out->dark = strstr(gtk_theme,":dark")!=NULL;
#endif
}

void se_design_style_native_window(const void* win32_hwnd, void* x11_display, unsigned long x11_window,
                                   bool dark, uint32_t caption_rgb, uint32_t caption_text_rgb){
#if defined(_WIN32)
  typedef HRESULT (WINAPI *se_dwm_set_window_attribute_t)(HWND, DWORD, LPCVOID, DWORD);
  static se_dwm_set_window_attribute_t dwm_set_window_attribute = NULL;
  static bool loaded = false;
  if(!loaded){
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if(dwm)dwm_set_window_attribute = (se_dwm_set_window_attribute_t)(void*)GetProcAddress(dwm,"DwmSetWindowAttribute");
    loaded = true;
  }
  if(!dwm_set_window_attribute||!win32_hwnd)return;
  HWND hwnd = (HWND)win32_hwnd;
  BOOL use_dark = dark;
  // DWMWA_USE_IMMERSIVE_DARK_MODE is 20, builds before Windows 10 20H1 used 19
  if(FAILED(dwm_set_window_attribute(hwnd,20,&use_dark,sizeof(use_dark))))
    dwm_set_window_attribute(hwnd,19,&use_dark,sizeof(use_dark));
  // DWMWA_CAPTION_COLOR / DWMWA_TEXT_COLOR (Windows 11), COLORREF is 0x00BBGGRR.
  // SE_ACCENT_NONE maps to DWMWA_COLOR_DEFAULT (0xFFFFFFFF) and restores the system colors.
  COLORREF caption = caption_rgb==SE_ACCENT_NONE? 0xFFFFFFFF : RGB((caption_rgb>>16)&0xff,(caption_rgb>>8)&0xff,caption_rgb&0xff);
  COLORREF text = caption_text_rgb==SE_ACCENT_NONE? 0xFFFFFFFF : RGB((caption_text_rgb>>16)&0xff,(caption_text_rgb>>8)&0xff,caption_text_rgb&0xff);
  dwm_set_window_attribute(hwnd,35,&caption,sizeof(caption));
  dwm_set_window_attribute(hwnd,36,&text,sizeof(text));
#elif defined(SE_DESIGN_FREEDESKTOP)
  if(!x11_display||!x11_window)return;
  // GNOME Shell (and Mutter under XWayland) picks the decoration style from this property
  Display* display = (Display*)x11_display;
  Atom variant = XInternAtom(display,"_GTK_THEME_VARIANT",False);
  Atom utf8 = XInternAtom(display,"UTF8_STRING",False);
  const char* value = dark? "dark" : "light";
  XChangeProperty(display,(Window)x11_window,variant,utf8,8,PropModeReplace,(const unsigned char*)value,(int)strlen(value));
  XFlush(display);
#endif
  (void)win32_hwnd; (void)x11_display; (void)x11_window;
  (void)dark; (void)caption_rgb; (void)caption_text_rgb;
}

static uint32_t se_read_be32(const uint8_t* p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static uint16_t se_read_be16(const uint8_t* p){return (uint16_t)((p[0]<<8)|p[1]);}

bool se_design_font_is_supported(const uint8_t* data, size_t size){
  // Mirrors the checks of stb_truetype's stbtt_InitFont, a font it rejects would
  // make Dear ImGui fail to build the whole font atlas.
  if(!data||size<12||size>(64u<<20))return false;
  uint32_t version = se_read_be32(data);
  if(version!=0x00010000&&version!=0x74727565/*true*/&&version!=0x4F54544F/*OTTO*/)return false;
  uint32_t num_tables = se_read_be16(data+4);
  if(12+num_tables*16>size)return false;
  uint32_t cmap_off = 0, cmap_len = 0;
  bool head=false, hhea=false, hmtx=false, glyf=false, loca=false, cff=false;
  for(uint32_t i=0;i<num_tables;++i){
    const uint8_t* rec = data+12+i*16;
    uint32_t tag = se_read_be32(rec), off = se_read_be32(rec+8), len = se_read_be32(rec+12);
    if(off>size||len>size-off)return false;
    switch(tag){
      case 0x636D6170: cmap_off = off; cmap_len = len; break; // cmap
      case 0x68656164: head = true; break; // head
      case 0x68686561: hhea = true; break; // hhea
      case 0x686D7478: hmtx = true; break; // hmtx
      case 0x676C7966: glyf = true; break; // glyf
      case 0x6C6F6361: loca = true; break; // loca
      case 0x43464620: cff = true; break;  // "CFF " (CFF2 variable fonts are not supported)
    }
  }
  if(!cmap_off||cmap_len<4||!head||!hhea||!hmtx)return false;
  if(!(glyf&&loca)&&!cff)return false;
  // Needs a Unicode character map
  uint32_t num_maps = se_read_be16(data+cmap_off+2);
  if(4+num_maps*8>cmap_len)return false;
  for(uint32_t i=0;i<num_maps;++i){
    const uint8_t* rec = data+cmap_off+4+i*8;
    uint16_t platform = se_read_be16(rec), encoding = se_read_be16(rec+2);
    if(platform==0)return true;
    if(platform==3&&(encoding==1||encoding==10))return true;
  }
  return false;
}

static bool se_design_font_file_is_supported(const char* path){
  FILE* f = fopen(path,"rb");
  if(!f)return false;
  fseek(f,0,SEEK_END);
  long size = ftell(f);
  fseek(f,0,SEEK_SET);
  bool ok = false;
  if(size>0&&size<(64l<<20)){
    uint8_t* data = (uint8_t*)malloc((size_t)size);
    if(data&&fread(data,1,(size_t)size,f)==(size_t)size)ok = se_design_font_is_supported(data,(size_t)size);
    free(data);
  }
  fclose(f);
  return ok;
}

bool se_design_find_system_font(int design, char* path, size_t path_size){
  if(!path||path_size==0)return false;
  path[0]=0;
  design = se_design_resolve(design);
  const char* const* candidates = NULL;
#if defined(_WIN32)
  char windows_fonts[3][260];
  const char* windir = getenv("WINDIR");
  if(!windir)windir = "C:\\Windows";
  snprintf(windows_fonts[0],sizeof(windows_fonts[0]),"%s\\Fonts\\SegUIVar.ttf",windir); // Segoe UI Variable (Windows 11)
  snprintf(windows_fonts[1],sizeof(windows_fonts[1]),"%s\\Fonts\\segoeui.ttf",windir);  // Segoe UI (Windows 10)
  const char* fluent_fonts[]={windows_fonts[0],windows_fonts[1],NULL};
  const char* material_fonts[]={NULL};
  const char* adwaita_fonts[]={NULL};
#else
  const char* fluent_fonts[]={NULL};
  const char* material_fonts[]={
    "/system/fonts/RobotoStatic-Regular.ttf",
    "/system/fonts/Roboto-Regular.ttf",
    "/usr/share/fonts/truetype/roboto/unhinted/RobotoTTF/Roboto-Regular.ttf",
    "/usr/share/fonts/truetype/roboto/hinted/Roboto-Regular.ttf",
    "/usr/share/fonts/google-roboto/Roboto-Regular.ttf",
    "/usr/share/fonts/TTF/Roboto-Regular.ttf",
    "/usr/local/share/fonts/roboto/Roboto-Regular.ttf",
    NULL
  };
  const char* adwaita_fonts[]={
    "/usr/share/fonts/adwaita-sans-fonts/AdwaitaSans-Regular.ttf",
    "/usr/share/fonts/truetype/adwaita-sans/AdwaitaSans-Regular.ttf",
    "/usr/share/fonts/truetype/adwaita/AdwaitaSans-Regular.ttf",
    "/usr/share/fonts/adwaita/AdwaitaSans-Regular.ttf",
    "/usr/share/fonts/TTF/AdwaitaSans-Regular.ttf",
    "/usr/local/share/fonts/adwaita/AdwaitaSans-Regular.ttf",
    "/usr/share/fonts/cantarell/Cantarell-Regular.otf",
    "/usr/share/fonts/opentype/cantarell/Cantarell-Regular.otf",
    "/usr/share/fonts/OTF/Cantarell-Regular.otf",
    "/usr/local/share/fonts/cantarell/Cantarell-Regular.otf",
    NULL
  };
#endif
  switch(design){
    case SE_DESIGN_FLUENT:    candidates = fluent_fonts; break;
    case SE_DESIGN_ADWAITA:   candidates = adwaita_fonts; break;
    case SE_DESIGN_MATERIAL3: candidates = material_fonts; break;
    default: return false;
  }
#if defined(SE_DESIGN_FREEDESKTOP)
  if(design==SE_DESIGN_ADWAITA){
    // Honor the interface font configured in GNOME when fontconfig can resolve it
    FILE* f = popen("sh -c 'n=$(gsettings get org.gnome.desktop.interface font-name 2>/dev/null | tr -d \"\\047\" | sed \"s/ [0-9.]*$//\");"
                    " [ -n \"$n\" ] && fc-match -f \"%{file}\" \"$n\"' 2>/dev/null","r");
    if(f){
      char found[1024]={0};
      if(fgets(found,sizeof(found),f)){
        found[strcspn(found,"\r\n")]=0;
        if(found[0]&&se_design_font_file_is_supported(found)&&strlen(found)<path_size){
          strncpy(path,found,path_size);
          pclose(f);
          return true;
        }
      }
      pclose(f);
    }
  }
#endif
  for(int i=0;candidates[i];++i){
    if(se_design_font_file_is_supported(candidates[i])&&strlen(candidates[i])<path_size){
      strncpy(path,candidates[i],path_size);
      return true;
    }
  }
  return false;
}
