/*****************************************************************************
 *
 *   Unit tests of the recorder (src/se_record.c)
 *
 *   cc -O2 -Wall -Wextra -Isrc tools/se_record_test.c src/se_record.c src/stb.c -lm -o se_record_test
 *
 *   The AVI files are read back: RIFF and list sizes, headers, the index, every frame (decoded
 *   with stb_image for MJPEG) and every audio sample are checked against what was written.
 *
**/
#include "se_record.h"
#include "stb_image.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, ...) do{ if(!(cond)){ failures++; printf("FAIL %s:%d: ",__FILE__,__LINE__); printf(__VA_ARGS__); printf("\n"); } }while(0)

static char dir[1024];

static uint32_t rd32(const uint8_t* p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint16_t rd16(const uint8_t* p){return (uint16_t)(p[0]|(p[1]<<8));}

static uint8_t* read_file(const char* path, size_t* size){
  FILE* f = fopen(path,"rb");
  if(!f)return NULL;
  fseek(f,0,SEEK_END);
  long s = ftell(f);
  fseek(f,0,SEEK_SET);
  uint8_t* data = (uint8_t*)malloc(s? s : 1);
  if(fread(data,1,s,f)!=(size_t)s){fclose(f); free(data); return NULL;}
  fclose(f);
  *size = (size_t)s;
  return data;
}

// Test pattern: a gradient that moves with the frame number, so every frame is different
static void make_frame(uint8_t* rgba, int w, int h, int frame){
  for(int y=0;y<h;++y)for(int x=0;x<w;++x){
    uint8_t* p = rgba+(y*w+x)*4;
    p[0] = (uint8_t)(x*255/(w-1));
    p[1] = (uint8_t)(y*255/(h-1));
    p[2] = (uint8_t)(frame*7);
    p[3] = 255;
  }
}

static int16_t sample_value(uint64_t i, int channel){
  return (int16_t)(sin((double)i*2*3.14159265*440.0/48000.0+channel)*12000);
}

typedef struct{
  int frames, audio_frames;
  uint32_t avih_frames, video_length, audio_length, streams, width, height;
  uint32_t scale, rate;
  bool mjpeg;
  int first_frame;   // Frame number of the first video chunk, from its pixels
}avi_info_t;

// Reads an AVI file back and checks its structure. Video chunks are compared with make_frame()
// frames starting at expect_frame (-1: detect), audio with sample_value() from expect_audio.
static bool check_avi(const char* path, int w, int h, int scale, int quality, int expect_frame, uint64_t expect_audio,
                      avi_info_t* info){
  memset(info,0,sizeof(*info));
  size_t size = 0;
  uint8_t* d = read_file(path,&size);
  if(!d){CHECK(0,"could not read %s",path); return false;}
  bool ok = true;
  #define REQ(c,...) do{ if(!(c)){ CHECK(0,__VA_ARGS__); ok = false; goto done; } }while(0)
  REQ(size>=12&&memcmp(d,"RIFF",4)==0&&memcmp(d+8,"AVI ",4)==0,"%s: RIFF AVI header",path);
  REQ(rd32(d+4)==size-8,"%s: RIFF size %u vs %zu",path,rd32(d+4),size-8);
  REQ(memcmp(d+12,"LIST",4)==0&&memcmp(d+20,"hdrl",4)==0,"hdrl list");
  size_t hdrl_end = 20+rd32(d+16);
  REQ(memcmp(d+24,"avih",4)==0&&rd32(d+28)==56,"avih");
  info->avih_frames = rd32(d+32+16);
  info->streams = rd32(d+32+24);
  info->width = rd32(d+32+32);
  info->height = rd32(d+32+36);
  // Stream lists
  size_t o = 88;
  for(uint32_t s=0;s<info->streams;++s){
    REQ(memcmp(d+o,"LIST",4)==0&&memcmp(d+o+8,"strl",4)==0,"strl %u",s);
    size_t end = o+8+rd32(d+o+4);
    const uint8_t* strh = d+o+12;
    REQ(memcmp(strh,"strh",4)==0,"strh");
    const uint8_t* sh = strh+8;
    const uint8_t* strf = strh+8+rd32(strh+4);
    REQ(memcmp(strf,"strf",4)==0,"strf");
    if(memcmp(sh,"vids",4)==0){
      info->mjpeg = memcmp(sh+4,"MJPG",4)==0;
      info->scale = rd32(sh+20);
      info->rate = rd32(sh+24);
      info->video_length = rd32(sh+32);
      REQ(rd32(strf+8+4)==(uint32_t)(w*scale)&&rd32(strf+8+8)==(uint32_t)(h*scale),"BITMAPINFOHEADER size");
      REQ(rd16(strf+8+14)==24,"24 bit");
    }else{
      REQ(memcmp(sh,"auds",4)==0,"audio stream type");
      info->audio_length = rd32(sh+32);
      REQ(rd16(strf+8)==1&&rd16(strf+10)==2&&rd32(strf+12)==48000&&rd16(strf+22)==16,"PCM 48 kHz stereo 16 bit");
    }
    o = end;
  }
  REQ(o==hdrl_end,"hdrl size %zu vs %zu",o,hdrl_end);
  REQ(memcmp(d+o,"LIST",4)==0&&memcmp(d+o+8,"movi",4)==0,"movi list at %zu",o);
  size_t movi = o+8;            // Index offsets are relative to the "movi" fourcc
  size_t movi_end = o+8+rd32(d+o+4);
  REQ(memcmp(d+movi_end,"idx1",4)==0,"idx1 after movi");
  uint32_t entries = rd32(d+movi_end+4)/16;
  REQ(movi_end+8+entries*16==size,"idx1 is the end of the file");
  // Walk the movi chunks and the index together
  size_t c = movi+4;
  uint64_t audio_index = expect_audio;
  int video_frame = expect_frame;
  for(uint32_t e=0;e<entries;++e){
    const uint8_t* ie = d+movi_end+8+e*16;
    REQ(c+8<=movi_end,"chunk %u inside movi",e);
    REQ(rd32(ie)==rd32(d+c),"index %u id matches chunk",e);
    REQ(rd32(ie+8)==c-movi,"index %u offset %u vs %zu",e,rd32(ie+8),c-movi);
    uint32_t csize = rd32(d+c+4);
    REQ(rd32(ie+12)==csize,"index %u size",e);
    REQ(rd32(ie+4)==0x10,"keyframe flag");
    const uint8_t* payload = d+c+8;
    if(memcmp(d+c,"01wb",4)==0){
      for(uint32_t i=0;i<csize/4;++i,++audio_index){
        int16_t l = (int16_t)rd16(payload+i*4), r = (int16_t)rd16(payload+i*4+2);
        if(l!=sample_value(audio_index,0)||r!=sample_value(audio_index,1)){
          CHECK(0,"%s: audio sample %llu",path,(unsigned long long)audio_index); ok = false; goto done;
        }
      }
      info->audio_frames+=csize/4;
    }else{
      REQ(memcmp(d+c,quality>0? "00dc" : "00db",4)==0,"video chunk id");
      int sw = w*scale, shh = h*scale;
      uint8_t* pixels = NULL;
      int iw = sw, ih = shh;
      if(quality>0){
        int comp;
        pixels = stbi_load_from_memory(payload,(int)csize,&iw,&ih,&comp,3);
        REQ(pixels&&iw==sw&&ih==shh,"JPEG frame decodes to %dx%d",sw,shh);
      }else{
        size_t stride = ((size_t)sw*3+3)&~(size_t)3;
        REQ(csize==stride*shh,"uncompressed frame size");
        pixels = (uint8_t*)malloc((size_t)sw*shh*3);
        for(int y=0;y<shh;++y)for(int x=0;x<sw;++x){
          const uint8_t* p = payload+(size_t)(shh-1-y)*stride+x*3;
          uint8_t* q = pixels+((size_t)y*sw+x)*3;
          q[0] = p[2]; q[1] = p[1]; q[2] = p[0];
        }
      }
      // Frame number from the blue channel (frame*7), then compare with the expected frame
      if(video_frame<0){
        int blue = pixels[2];
        for(int f=0;f<256;++f)if(abs((uint8_t)(f*7)-blue)<=3){video_frame = f; break;}
        info->first_frame = video_frame;
      }
      uint8_t* ref = (uint8_t*)malloc((size_t)w*h*4);
      make_frame(ref,w,h,video_frame);
      double err = 0;
      int max_err = 0;
      for(int y=0;y<shh;++y)for(int x=0;x<sw;++x)for(int k=0;k<3;++k){
        int diff = abs((int)pixels[((size_t)y*sw+x)*3+k]-(int)ref[((size_t)(y/scale)*w+x/scale)*4+k]);
        err+=diff*diff;
        if(diff>max_err)max_err = diff;
      }
      double mse = err/((double)sw*shh*3);
      double psnr = mse>0? 10*log10(255.0*255.0/mse) : 99;
      free(ref);
      if(quality>0)stbi_image_free(pixels); else free(pixels);
      if(quality>0)REQ(psnr>38,"%s: frame %d PSNR %.1f dB",path,video_frame,psnr);
      else REQ(max_err==0,"%s: uncompressed frame %d differs",path,video_frame);
      info->frames++;
      video_frame++;
    }
    c+=8+csize+(csize&1);
  }
  REQ(c==movi_end,"every movi chunk is indexed");
  #undef REQ
done:
  free(d);
  return ok;
}

static void write_audio_for_frame(se_avi_writer_t* avi, uint64_t* next, uint32_t frames){
  int16_t buffer[2*2048];
  for(uint32_t i=0;i<frames;++i){
    buffer[i*2] = sample_value(*next+i,0);
    buffer[i*2+1] = sample_value(*next+i,1);
  }
  CHECK(se_avi_add_audio(avi,buffer,frames),"add audio");
  *next+=frames;
}

static void test_part_paths(void){
  char out[256];
  se_record_part_path("a/b.avi",1,out,sizeof(out));
  CHECK(strcmp(out,"a/b.avi")==0,"part 1 keeps the name (%s)",out);
  se_record_part_path("a/b.avi",2,out,sizeof(out));
  CHECK(strcmp(out,"a/b (2).avi")==0,"part 2 (%s)",out);
  se_record_part_path("dir.v1/file",3,out,sizeof(out));
  CHECK(strcmp(out,"dir.v1/file (3)")==0,"no extension (%s)",out);
  se_record_part_path("C:\\games.d\\Game 1.wav",2,out,sizeof(out));
  CHECK(strcmp(out,"C:\\games.d\\Game 1 (2).wav")==0,"Windows path (%s)",out);
}

static void test_encode(void){
  se_record_buffer_t scratch = {0}, out = {0};
  // Uncompressed: 7x5 scaled by 2 is 14 pixels wide, 42 bytes padded to 44
  uint8_t rgba[7*5*4];
  for(int i=0;i<7*5;++i){rgba[i*4]=i; rgba[i*4+1]=100+i; rgba[i*4+2]=200-i; rgba[i*4+3]=255;}
  CHECK(se_record_encode_frame(rgba,7,5,2,0,&scratch,&out),"encode uncompressed");
  CHECK(out.size==44*10,"uncompressed size %zu",out.size);
  // Bottom row of the output is the top row of the image, BGR
  CHECK(out.data[9*44+0]==200&&out.data[9*44+1]==100&&out.data[9*44+2]==0,"first pixel at the bottom, BGR");
  CHECK(out.data[9*44+3]==200&&out.data[9*44+6]==199,"doubled horizontally");
  CHECK(out.data[42]==0&&out.data[43]==0,"row padding");
  // JPEG
  uint8_t frame[240*160*4];
  make_frame(frame,240,160,3);
  CHECK(se_record_encode_frame(frame,240,160,3,95,&scratch,&out),"encode JPEG");
  int w, h, comp;
  uint8_t* decoded = stbi_load_from_memory(out.data,(int)out.size,&w,&h,&comp,3);
  CHECK(decoded&&w==720&&h==480,"JPEG decodes at 3x");
  stbi_image_free(decoded);
  CHECK(!se_record_encode_frame(frame,0,160,1,95,&scratch,&out),"empty frame refused");
  se_record_buffer_free(&scratch);
  se_record_buffer_free(&out);
}

static void test_avi(int quality, int scale, uint64_t split, const char* name){
  const int W = 40, H = 24, N = 90;
  char path[1100];
  snprintf(path,sizeof(path),"%s/%s.avi",dir,name);
  se_avi_format_t format = {W*scale,H*scale,quality,16777216,280896,48000,2,split};
  se_avi_writer_t* avi = se_avi_open(path,&format);
  CHECK(avi!=NULL,"open %s",path);
  if(!avi)return;
  se_record_buffer_t scratch = {0}, out = {0};
  uint8_t frame[40*24*4];
  uint64_t audio = 0;
  for(int f=0;f<N;++f){
    // 803 or 804 samples per frame, like 48 kHz at 59.73 frames per second
    write_audio_for_frame(avi,&audio,803+(f%3==0));
    make_frame(frame,W,H,f);
    CHECK(se_record_encode_frame(frame,W,H,scale,quality,&scratch,&out),"encode");
    CHECK(se_avi_add_frame(avi,out.data,out.size),"add frame %d",f);
  }
  write_audio_for_frame(avi,&audio,500);   // Audio after the last frame is written on close
  CHECK(se_avi_frames(avi)==N,"frame count");
  int parts = se_avi_parts(avi);
  uint64_t bytes = se_avi_bytes(avi);
  CHECK(se_avi_error(avi)==NULL,"no error");
  CHECK(se_avi_close(avi),"close");
  se_record_buffer_free(&scratch);
  se_record_buffer_free(&out);
  int frames = 0;
  uint64_t audio_read = 0, file_bytes = 0;
  for(int p=1;p<=parts;++p){
    char part[1200];
    se_record_part_path(path,p,part,sizeof(part));
    avi_info_t info;
    CHECK(check_avi(part,W,H,scale,quality,frames,audio_read,&info),"%s part %d is valid",name,p);
    CHECK(info.avih_frames==(uint32_t)info.frames&&info.video_length==(uint32_t)info.frames,"part %d frame counts",p);
    CHECK(info.audio_length==(uint32_t)info.audio_frames,"part %d audio length %u vs %d",p,info.audio_length,info.audio_frames);
    CHECK(info.streams==2&&info.mjpeg==(quality>0),"part %d streams",p);
    CHECK(info.scale==280896&&info.rate==16777216,"frame rate");
    CHECK(info.width==(uint32_t)(W*scale)&&info.height==(uint32_t)(H*scale),"avih size");
    frames+=info.frames;
    audio_read+=info.audio_frames;
    size_t s = 0;
    uint8_t* d = read_file(part,&s);
    free(d);
    file_bytes+=s;
    if(split)CHECK(s<=split,"part %d is %zu bytes, split at %llu",p,s,(unsigned long long)split);
    remove(part);
  }
  CHECK(frames==N,"%s: %d frames read back",name,frames);
  CHECK(audio_read==audio,"%s: %llu of %llu audio samples read back",name,(unsigned long long)audio_read,(unsigned long long)audio);
  // Counted before closing: the index and the audio after the last frame come on top
  CHECK(file_bytes>bytes&&file_bytes-bytes<=(uint64_t)parts*(N*2+1)*16+8*parts+2008,"byte count %llu vs %llu",
        (unsigned long long)file_bytes,(unsigned long long)bytes);
  if(split)CHECK(parts>1,"%s: split into %d parts",name,parts);
  else CHECK(parts==1,"one part");
}

static void test_wav(void){
  char path[1100];
  snprintf(path,sizeof(path),"%s/audio.wav",dir);
  se_wav_writer_t* wav = se_wav_open(path,48000,2);
  CHECK(wav!=NULL,"open wav");
  if(!wav)return;
  int16_t buffer[2*1000];
  uint64_t n = 0;
  for(int block=0;block<10;++block){
    for(int i=0;i<1000;++i){buffer[i*2]=sample_value(n+i,0); buffer[i*2+1]=sample_value(n+i,1);}
    CHECK(se_wav_add_audio(wav,buffer,1000),"write wav");
    n+=1000;
  }
  CHECK(se_wav_frames(wav)==10000,"wav frame count");
  CHECK(se_wav_close(wav),"close wav");
  size_t size = 0;
  uint8_t* d = read_file(path,&size);
  CHECK(d&&size==44+40000,"wav size %zu",size);
  if(d){
    CHECK(memcmp(d,"RIFF",4)==0&&rd32(d+4)==size-8&&memcmp(d+8,"WAVEfmt ",8)==0,"wav header");
    CHECK(rd16(d+20)==1&&rd16(d+22)==2&&rd32(d+24)==48000&&rd16(d+34)==16,"wav format");
    CHECK(memcmp(d+36,"data",4)==0&&rd32(d+40)==40000,"wav data size");
    bool same = true;
    for(int i=0;i<10000;++i)same&=(int16_t)rd16(d+44+i*4)==sample_value(i,0)&&(int16_t)rd16(d+46+i*4)==sample_value(i,1);
    CHECK(same,"wav samples");
  }
  free(d);
  remove(path);
}

static void test_replay(void){
  const int W = 32, H = 16, CAP = 30, N = 100;
  se_replay_buffer_t replay = {0};
  CHECK(se_replay_init(&replay,CAP,W,H,0,2),"init replay");
  se_record_buffer_t scratch = {0}, out = {0};
  uint8_t frame[32*16*4];
  int16_t audio[2*800];
  uint64_t n = 0;
  for(int f=0;f<N;++f){
    make_frame(frame,W,H,f);
    se_record_encode_frame(frame,W,H,1,0,&scratch,&out);
    for(int i=0;i<800;++i){audio[i*2]=sample_value(n+i,0); audio[i*2+1]=sample_value(n+i,1);}
    n+=800;
    CHECK(se_replay_push(&replay,out.data,out.size,audio,800),"push");
  }
  CHECK(replay.count==CAP,"replay keeps %d frames",CAP);
  char path[1100];
  snprintf(path,sizeof(path),"%s/replay.avi",dir);
  se_avi_format_t format = {W,H,0,16777216,280896,48000,2,0};
  CHECK(se_replay_save(&replay,path,&format),"save replay");
  avi_info_t info;
  CHECK(check_avi(path,W,H,1,0,N-CAP,(uint64_t)(N-CAP)*800,&info),"replay file is valid");
  CHECK(info.frames==CAP&&info.audio_frames==CAP*800,"replay has the last %d frames and their audio",CAP);
  remove(path);
  se_replay_clear(&replay);
  CHECK(!se_replay_save(&replay,path,&format),"empty replay is not saved");
  se_replay_free(&replay);
  se_record_buffer_free(&scratch);
  se_record_buffer_free(&out);
}

static void test_errors(void){
  se_avi_format_t format = {16,16,90,60,1,48000,2,0};
  char path[1100];
  snprintf(path,sizeof(path),"%s/missing/dir/x.avi",dir);
  CHECK(se_avi_open(path,&format)==NULL,"no file in a missing folder");
  CHECK(se_wav_open(path,48000,2)==NULL,"no wav in a missing folder");
  se_avi_format_t bad = format;
  bad.fps_num = 0;
  snprintf(path,sizeof(path),"%s/bad.avi",dir);
  CHECK(se_avi_open(path,&bad)==NULL,"frame rate required");
}

int main(int argc, char** argv){
  snprintf(dir,sizeof(dir),"%s",argc>1? argv[1] : ".");
  test_part_paths();
  test_encode();
  test_avi(95,1,0,"mjpeg");
  test_avi(95,2,0,"mjpeg2x");
  test_avi(0,1,0,"raw");
  test_avi(0,3,0,"raw3x");
  test_avi(0,2,120*1024,"split");
  test_avi(92,4,60*1024,"split_mjpeg");
  test_wav();
  test_replay();
  test_errors();
  if(failures){
    printf("%d recorder checks failed\n",failures);
    return 1;
  }
  printf("All recorder checks passed\n");
  return 0;
}
