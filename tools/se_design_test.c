// Unit test for the design token engine (src/se_design.c).
// Build and run from the repository root:
//   cc -O2 -Isrc tools/se_design_test.c src/se_design.c -lm -o se_design_test && ./se_design_test
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "se_design.h"

static int failures = 0;
#define CHECK(cond, ...) do{ if(!(cond)){ printf("FAIL %s:%d: ",__FILE__,__LINE__); printf(__VA_ARGS__); printf("\n"); failures++; } }while(0)

// Distance between two sRGB colors in 8 bit units (max channel difference)
static int rgb_distance(uint32_t a, uint32_t b){
  int d = 0;
  for(int s=0;s<24;s+=8){
    int ca = (a>>s)&0xff, cb = (b>>s)&0xff;
    int diff = ca>cb? ca-cb : cb-ca;
    if(diff>d)d=diff;
  }
  return d;
}

static void test_tones(void){
  CHECK(fabsf(se_color_tone(0x000000))<0.01f,"black tone");
  CHECK(fabsf(se_color_tone(0xffffff)-100.f)<0.01f,"white tone");
  CHECK(fabsf(se_color_tone(0x777777)-50.f)<0.5f,"mid gray tone %f",se_color_tone(0x777777));
  // Every generated tone must land on the requested CIELAB L*
  se_tonal_palette_t p;
  se_tonal_palette_from_color(0x0078D4,&p);
  for(int i=0;i<SE_NUM_STANDARD_TONES;++i){
    float t = se_standard_tones[i];
    uint32_t c = se_tonal_palette_tone(&p,t);
    CHECK(fabsf(se_color_tone(c)-t)<0.75f,"tone %.0f of #0078D4 palette is %.2f (#%06X)",t,se_color_tone(c),c);
  }
  // Tone differences guarantee contrast: 40 vs 100 must pass WCAG AA (4.5:1)
  CHECK(se_contrast_ratio(se_tonal_palette_tone(&p,40),0xffffff)>=4.5f,"tone 40 on white contrast");
}

static void test_material_baseline(void){
  // Material 3 baseline scheme, generated from seed #6750A4
  se_core_palette_t core;
  se_core_palette_from_seed(0x6750A4,&core);
  struct{const se_tonal_palette_t* p; float tone; uint32_t expected; const char* name;}ref[]={
    {&core.primary,40,0x6750A4,"primary40"},
    {&core.primary,80,0xD0BCFF,"primary80"},
    {&core.primary,90,0xEADDFF,"primary90"},
    {&core.primary,10,0x21005D,"primary10"},
    {&core.secondary,40,0x625B71,"secondary40"},
    {&core.secondary,90,0xE8DEF8,"secondary90"},
    {&core.tertiary,40,0x7D5260,"tertiary40"},
    {&core.tertiary,90,0xFFD8E4,"tertiary90"},
    {&core.neutral,10,0x1D1B20,"neutral10"},
    {&core.neutral,6,0x141218,"neutral6"},
    {&core.neutral,94,0xF3EDF7,"neutral94"},
    {&core.neutral_variant,30,0x49454F,"neutral_variant30"},
    {&core.neutral_variant,50,0x79747E,"neutral_variant50"},
    {&core.neutral_variant,80,0xCAC4D0,"neutral_variant80"},
  };
  for(size_t i=0;i<sizeof(ref)/sizeof(ref[0]);++i){
    uint32_t got = se_tonal_palette_tone(ref[i].p,ref[i].tone);
    int d = rgb_distance(got,ref[i].expected);
    printf("  %-18s expected #%06X got #%06X (max channel diff %d)\n",ref[i].name,ref[i].expected,got,d);
    CHECK(d<=16,"%s too far from the Material baseline",ref[i].name);
  }
}

static void test_exact_system_palette(void){
  // Android system palettes must come back unchanged at the standard tones
  uint32_t tones[SE_NUM_STANDARD_TONES]={0xFFFFFF,0xFEFBFF,0xEEF0FF,0xD9E2FF,0xB0C6FF,0x86A9FF,
                                         0x5D8EF7,0x3F73DB,0x1E5AC0,0x00429C,0x002D6E,0x001945,0x000000};
  se_tonal_palette_t p;
  se_tonal_palette_from_tones(tones,&p);
  for(int i=0;i<SE_NUM_STANDARD_TONES;++i){
    CHECK(se_tonal_palette_tone(&p,se_standard_tones[i])==tones[i],"exact tone %d",i);
  }
  uint32_t between = se_tonal_palette_tone(&p,94);
  CHECK(fabsf(se_color_tone(between)-94.f)<0.75f,"generated tone 94");
}

static void test_scheme_contrast(void){
  // Text roles must stay readable on the surfaces they are drawn on
  int designs[]={SE_DESIGN_MATERIAL3,SE_DESIGN_FLUENT,SE_DESIGN_ADWAITA};
  int schemes[]={SE_COLOR_SCHEME_LIGHT,SE_COLOR_SCHEME_DARK,SE_COLOR_SCHEME_BLACK};
  uint32_t accents[]={SE_ACCENT_NONE,0xE62D42,0x3A944A,0xC88800,0x000080,0xFFFF00};
  for(size_t d=0;d<3;++d)for(size_t s=0;s<3;++s)for(size_t a=0;a<sizeof(accents)/sizeof(accents[0]);++a){
    se_design_tokens_t t;
    se_design_build_tokens(designs[d],schemes[s],accents[a],NULL,&t);
    CHECK(t.design==designs[d],"resolved design");
    CHECK(t.dark==(schemes[s]!=SE_COLOR_SCHEME_LIGHT),"dark flag");
    se_color_t panel = t.surface_panel;
    uint32_t text = se_color_to_rgb(se_color_blend(panel,t.on_surface));
    float c = se_contrast_ratio(text,se_color_to_rgb(panel));
    CHECK(c>=7.f,"%s scheme %d accent %06X: on_surface contrast %.2f",se_design_name(designs[d]),schemes[s],accents[a],c);
    uint32_t sel = se_color_to_rgb(se_color_blend(panel,t.selected));
    uint32_t on_sel = se_color_to_rgb(se_color_blend(se_color_from_rgb(sel,1),t.on_selected));
    c = se_contrast_ratio(on_sel,sel);
    CHECK(c>=3.f,"%s scheme %d accent %06X: selected contrast %.2f",se_design_name(designs[d]),schemes[s],accents[a],c);
    uint32_t prim = se_color_to_rgb(se_color_blend(panel,t.primary));
    uint32_t on_prim = se_color_to_rgb(t.on_primary);
    c = se_contrast_ratio(on_prim,prim);
    // libadwaita keeps white on the raw accent (e.g. yellow), everything else must pass
    if(designs[d]!=SE_DESIGN_ADWAITA)CHECK(c>=3.f,"%s scheme %d accent %06X: on_primary contrast %.2f",se_design_name(designs[d]),schemes[s],accents[a],c);
  }
}

static void test_system_appearance(void){
  se_system_appearance_t sys;
  memset(&sys,0,sizeof(sys));
  sys.dark = 0;
  sys.accent = 0xE62D42;
  se_design_tokens_t t;
  se_design_build_tokens(SE_DESIGN_ADWAITA,SE_COLOR_SCHEME_SYSTEM,SE_ACCENT_NONE,&sys,&t);
  CHECK(!t.dark,"follows system light mode");
  CHECK(t.accent==0xE62D42,"uses system accent");
  CHECK(se_color_to_rgb(t.primary)==0xE62D42,"Adwaita accent_bg is the accent");
  se_design_build_tokens(SE_DESIGN_ADWAITA,SE_COLOR_SCHEME_SYSTEM,0x3A944A,&sys,&t);
  CHECK(t.accent==0x3A944A,"custom accent wins over system accent");
  sys.dark = -1;
  se_design_build_tokens(SE_DESIGN_FLUENT,SE_COLOR_SCHEME_SYSTEM,SE_ACCENT_NONE,&sys,&t);
  CHECK(t.dark,"unknown system preference defaults to dark");
  se_design_build_tokens(SE_DESIGN_FLUENT,SE_COLOR_SCHEME_DARK,SE_ACCENT_NONE,NULL,&t);
  CHECK(se_color_to_rgb(t.primary)==0x60CDFF,"Windows default accent in dark mode is #60CDFF");
  se_design_build_tokens(SE_DESIGN_FLUENT,SE_COLOR_SCHEME_LIGHT,SE_ACCENT_NONE,NULL,&t);
  CHECK(se_color_to_rgb(t.primary)==0x005FB8,"Windows default accent in light mode is #005FB8");
  CHECK(se_design_resolve(SE_DESIGN_AUTO)==se_design_platform_default(),"auto resolves to platform");
  CHECK(se_design_resolve(99)==se_design_platform_default(),"invalid resolves to platform");
}

static void test_font_validation(void){
  uint8_t junk[64]={0};
  CHECK(!se_design_font_is_supported(junk,sizeof(junk)),"rejects junk");
  CHECK(!se_design_font_is_supported(NULL,0),"rejects NULL");
  // Minimal TrueType header with only a CFF2 table must be rejected
  uint8_t cff2[12+16]={0x00,0x01,0x00,0x00, 0x00,0x01, 0,0,0,0,0,0, 'C','F','F','2', 0,0,0,0, 0,0,0,28, 0,0,0,0};
  CHECK(!se_design_font_is_supported(cff2,sizeof(cff2)),"rejects CFF2 only fonts");
}

static void test_customization(void){
  int designs[]={SE_DESIGN_MATERIAL3,SE_DESIGN_FLUENT,SE_DESIGN_ADWAITA};
  uint32_t accents[]={0,0xffffff,0xffff00,0x000080,0xE62D42};
  for(size_t d=0;d<3;++d)for(int scheme=1;scheme<=3;++scheme)for(size_t a=0;a<5;++a){
    se_design_tokens_t t;
    se_design_build_tokens(designs[d],scheme,accents[a],NULL,&t);
    float rounding=t.button_rounding;
    se_design_customize(&t,true,2,0.5f);
    CHECK(t.button_rounding==rounding*0.5f,"custom roundness");
    CHECK(t.font_size+2*t.frame_padding_y>=48,"touch target >=48px");
    CHECK(t.surface_panel.a==1 && t.control_border==2,"opaque high contrast surfaces");
    CHECK(se_contrast_ratio(se_color_to_rgb(t.on_surface),se_color_to_rgb(t.surface_panel))>=20,"high contrast body text");
    CHECK(se_contrast_ratio(se_color_to_rgb(t.on_primary),se_color_to_rgb(t.primary))>=4.5f,"high contrast accent text");
    CHECK(se_contrast_ratio(se_color_to_rgb(t.on_selected),se_color_to_rgb(t.selected))>=4.5f,"high contrast selected text");
  }
  se_design_tokens_t t;
  se_design_build_tokens(SE_DESIGN_MATERIAL3,SE_COLOR_SCHEME_DARK,SE_ACCENT_NONE,NULL,&t);
  float padding=t.frame_padding_y;
  se_design_customize(&t,false,1,NAN);
  CHECK(isfinite(t.button_rounding) && t.frame_padding_y<padding,"compact density, finite rounding");
}

int main(void){
  test_tones();
  test_material_baseline();
  test_exact_system_palette();
  test_scheme_contrast();
  test_system_appearance();
  test_font_validation();
  test_customization();
  if(failures){printf("%d check(s) failed\n",failures);return 1;}
  printf("All design token checks passed\n");
  return 0;
}
