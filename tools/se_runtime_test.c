// Regression cases for persisted settings, exact file I/O and wrapped stereo audio.
#include "sb_types.h"
#include "se_settings.h"
#include "se_design.h"
#include <math.h>
#include <stddef.h>
#include <limits.h>
#ifdef _WIN32
#include <process.h>
#define process_id _getpid
#else
#include <unistd.h>
#define process_id getpid
#endif

static int failures;
#define CHECK(c) do { if(!(c)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#c);++failures;} } while(0)

static void test_settings(void){
  CHECK(sizeof(persistent_settings_t)==1024);
  CHECK(offsetof(persistent_settings_t,use_bundled_font)==168);
  CHECK(offsetof(persistent_settings_t,scanline_strength)==172);
  persistent_settings_t s;
  memset(&s,0xff,sizeof(s)); // Missing/unsupported settings file.
  se_settings_migrate(&s,true);
  CHECK(s.settings_file_version==SE_SETTINGS_VERSION);
  CHECK(s.volume==0.8f && s.only_one_notification==1);
  CHECK(s.screen_shader==SE_SHADER_SUBPIXELS && s.display_brightness==1);
  CHECK(s.gb_palette[0]==0x388F81 && s.gb_palette[3]==0x2D4A31);
  s.volume=0.25f; s.design_system=SE_DESIGN_FLUENT;
  s.custom_accent=0x123456; s.use_custom_accent=1;
  s.settings_file_version=4;
  s.scanline_strength=NAN; s.high_contrast=UINT_MAX; s.corner_radius_scale=INFINITY;
  se_settings_migrate(&s,false);
  CHECK(s.volume==0.25f && s.design_system==SE_DESIGN_FLUENT);
  CHECK(s.custom_accent==0x123456 && s.use_custom_accent==1);
  CHECK(s.scanline_strength==0.35f && s.high_contrast==0 && s.corner_radius_scale==1);
  persistent_settings_t migrated=s;
  se_settings_migrate(&s,false);
  CHECK(memcmp(&migrated,&s,sizeof(s))==0); // Migration is idempotent.
  s.volume=NAN; s.ghosting=INFINITY; s.color_correction=-INFINITY;
  s.gui_scale_factor=NAN; s.custom_font_scale=NAN; s.touch_controls_scale=NAN;
  s.display_brightness=INFINITY; s.display_saturation=-2; s.curvature=2;
  s.screen_shader=UINT_MAX; s.ui_density=UINT_MAX; s.screen_rotation=UINT_MAX;
  s.http_control_server_port=UINT_MAX;
  se_settings_validate(&s);
  CHECK(s.volume==0.8f && s.ghosting==1 && s.color_correction==1);
  CHECK(s.gui_scale_factor==1 && s.custom_font_scale==1 && s.touch_controls_scale==1);
  CHECK(s.display_brightness==1 && s.display_saturation==0 && s.curvature==0.25f);
  CHECK(s.screen_shader==SE_SHADER_SUBPIXELS && s.ui_density==0 && s.screen_rotation==0);
  CHECK(s.http_control_server_port==8080);
  for(uint32_t version=0;version<4;++version){
    memset(&s,0,sizeof(s));s.settings_file_version=version;s.theme=3;
    se_settings_migrate(&s,false);
    CHECK(s.design_system==SE_DESIGN_CLASSIC); // Custom image skins survive migration.
  }
  memset(&s,0,sizeof(s));s.settings_file_version=3;s.theme=1;
  se_settings_migrate(&s,false);CHECK(s.color_scheme==SE_COLOR_SCHEME_LIGHT);
  s.settings_file_version=3;s.theme=2;
  se_settings_migrate(&s,false);CHECK(s.color_scheme==SE_COLOR_SCHEME_BLACK);
  for(uint32_t shader=0;shader<SE_SHADER_COUNT;++shader){
    s.screen_shader=shader;se_settings_validate(&s);CHECK(s.screen_shader==shader);
  }
}

static void test_audio(void){
  sb_ring_buffer_t ring={0};
  ring.read_ptr=SB_AUDIO_RING_BUFFER_SIZE-2;ring.write_ptr=ring.read_ptr+6;
  ring.data[SB_AUDIO_RING_BUFFER_SIZE-2]=11;ring.data[SB_AUDIO_RING_BUFFER_SIZE-1]=12;
  ring.data[0]=21;ring.data[1]=22;ring.data[2]=31;ring.data[3]=32;
  const int16_t* data;
  CHECK(sb_ring_buffer_size(&ring)==6);
  CHECK(sb_ring_buffer_read_span(&ring,&data)==2 && data[0]==11 && data[1]==12);
  ring.read_ptr+=2;
  CHECK(sb_ring_buffer_read_span(&ring,&data)==4 && data[0]==21 && data[3]==32);
  ring.read_ptr+=2; // Frontend consumed only one of the two offered frames.
  CHECK(sb_ring_buffer_read_span(&ring,&data)==2 && data[0]==31);
  ring.read_ptr+=2;
  CHECK(sb_ring_buffer_size(&ring)==0);
  ring.read_ptr=UINT32_MAX-1;ring.write_ptr=4;
  CHECK(sb_ring_buffer_size(&ring)==6);
  CHECK(sb_ring_buffer_read_span(&ring,&data)==2);
  ring.read_ptr+=2;
  CHECK(ring.read_ptr==0 && sb_ring_buffer_read_span(&ring,&data)==4);
  uint32_t read=ring.read_ptr,write=ring.write_ptr;
  (void)sb_ring_buffer_size(&ring);
  CHECK(ring.read_ptr==read && ring.write_ptr==write);
}

static void test_files(void){
  char path[128];snprintf(path,sizeof(path),"se-runtime-fixture-%d.bin",process_id());
  const uint8_t expected[]={0,1,2,255};uint8_t out[4]={0};size_t size=999;
  CHECK(sb_save_file_data(path,expected,sizeof(expected)));
  CHECK(sb_load_file_data_into_buffer(path,out,sizeof(out)) && memcmp(out,expected,4)==0);
  // Wrong-size reads must close their handle (thousands of corrupt configs).
  for(int i=0;i<4096;++i)CHECK(!sb_load_file_data_into_buffer(path,out,3));
  uint8_t* loaded=sb_load_file_data(path,&size);
  CHECK(loaded && size==4 && memcmp(loaded,expected,4)==0);free(loaded);
  loaded=sb_load_file_data(path,NULL);CHECK(loaded && loaded[3]==255);free(loaded);
  CHECK(sb_save_file_data(path,NULL,0));
  loaded=sb_load_file_data(path,&size);CHECK(loaded && size==0);free(loaded);
  CHECK(sb_load_file_data_into_buffer(path,NULL,0));
  CHECK(!sb_load_file_data_into_buffer(path,NULL,1));
  CHECK(!sb_save_file_data(path,NULL,1));
  CHECK(remove(path)==0);
  CHECK(sb_load_file_data(path,&size)==NULL && size==0);
  CHECK(sb_load_file_data(NULL,NULL)==NULL);
#ifndef _WIN32
  CHECK(!sb_save_file_data("/dev/full",expected,4)); // Buffered writes fail at close.
#endif
}

int main(void){
  test_settings();test_audio();test_files();
  if(failures){fprintf(stderr,"%d failures\n",failures);return 1;}
  puts("Settings, audio wraparound and file I/O regression checks passed");return 0;
}
