/*****************************************************************************
 *
 *   SkyEmu recording, see se_record.h
 *
**/
#include "se_record.h"
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SE_AVI_DEFAULT_SPLIT (1024ull*1024ull*1024ull)
#define SE_WAV_SPLIT (2048ull*1024ull*1024ull)
#define SE_AVIIF_KEYFRAME 0x10u

bool se_record_buffer_reserve(se_record_buffer_t* buffer, size_t capacity){
  if(capacity<=buffer->capacity)return true;
  size_t new_capacity = buffer->capacity? buffer->capacity : 4096;
  while(new_capacity<capacity)new_capacity*=2;
  uint8_t* data = (uint8_t*)realloc(buffer->data,new_capacity);
  if(!data)return false;
  buffer->data = data;
  buffer->capacity = new_capacity;
  return true;
}
void se_record_buffer_free(se_record_buffer_t* buffer){
  free(buffer->data);
  memset(buffer,0,sizeof(*buffer));
}
static bool se_record_buffer_append(se_record_buffer_t* buffer, const void* data, size_t size){
  if(!se_record_buffer_reserve(buffer,buffer->size+size))return false;
  memcpy(buffer->data+buffer->size,data,size);
  buffer->size+=size;
  return true;
}

typedef struct{
  se_record_buffer_t* out;
  bool failed;
}se_record_jpeg_context_t;
static void se_record_jpeg_write(void* context, void* data, int size){
  se_record_jpeg_context_t* c = (se_record_jpeg_context_t*)context;
  if(!se_record_buffer_append(c->out,data,(size_t)size))c->failed = true;
}

bool se_record_encode_frame(const uint8_t* rgba, int width, int height, int scale, int quality,
                            se_record_buffer_t* scratch, se_record_buffer_t* out){
  if(scale<1)scale = 1;
  int w = width*scale, h = height*scale;
  out->size = 0;
  if(w<=0||h<=0)return false;
  if(quality<=0){
    // Uncompressed DIB: bottom-up BGR, rows padded to 4 bytes
    size_t stride = ((size_t)w*3+3)&~(size_t)3;
    if(!se_record_buffer_reserve(out,stride*h))return false;
    out->size = stride*h;
    for(int y=0;y<h;++y){
      const uint8_t* src = rgba+(size_t)(y/scale)*width*4;
      uint8_t* dst = out->data+(size_t)(h-1-y)*stride;
      for(int x=0;x<w;++x){
        const uint8_t* p = src+(x/scale)*4;
        dst[x*3+0] = p[2];
        dst[x*3+1] = p[1];
        dst[x*3+2] = p[0];
      }
      memset(dst+(size_t)w*3,0,stride-(size_t)w*3);
    }
    return true;
  }
  if(!se_record_buffer_reserve(scratch,(size_t)w*h*3))return false;
  for(int y=0;y<h;++y){
    const uint8_t* src = rgba+(size_t)(y/scale)*width*4;
    uint8_t* dst = scratch->data+(size_t)y*w*3;
    for(int x=0;x<w;++x){
      const uint8_t* p = src+(x/scale)*4;
      dst[x*3+0] = p[0];
      dst[x*3+1] = p[1];
      dst[x*3+2] = p[2];
    }
  }
  se_record_jpeg_context_t context = {out,false};
  if(quality>100)quality = 100;
  if(!stbi_write_jpg_to_func(se_record_jpeg_write,&context,w,h,3,scratch->data,quality))return false;
  return !context.failed;
}

void se_record_part_path(const char* path, int part, char* out, size_t out_size){
  if(part<=1){
    snprintf(out,out_size,"%s",path);
    return;
  }
  const char* slash = strrchr(path,'/');
  const char* backslash = strrchr(path,'\\');
  if(backslash>slash)slash = backslash;
  const char* dot = strrchr(path,'.');
  if(!dot||(slash&&dot<slash))dot = path+strlen(path);
  snprintf(out,out_size,"%.*s (%d)%s",(int)(dot-path),path,part,dot);
}

static void se_put16(uint8_t* p, uint32_t v){p[0]=v&0xff; p[1]=(v>>8)&0xff;}
static void se_put32(uint8_t* p, uint32_t v){p[0]=v&0xff; p[1]=(v>>8)&0xff; p[2]=(v>>16)&0xff; p[3]=(v>>24)&0xff;}

// Writes a 32 bit little endian value at an earlier position of the file
static bool se_record_patch32(FILE* f, long position, uint32_t value){
  uint8_t b[4];
  se_put32(b,value);
  return fseek(f,position,SEEK_SET)==0&&fwrite(b,1,4,f)==4;
}

///////////////////
// AVI           //
///////////////////
typedef struct{
  uint32_t id, flags, offset, size;
}se_avi_index_t;

struct se_avi_writer_t{
  se_avi_format_t format;
  char path[4096];
  FILE* file;
  int part;
  const char* error;
  // Current part
  uint64_t file_size;
  long movi_list;               // Position of the movi LIST header
  long riff_size_pos, movi_size_pos, avih_frames_pos, avih_buffer_pos;
  long video_length_pos, video_buffer_pos, audio_length_pos;
  uint32_t part_frames, part_audio_frames, max_chunk;
  se_avi_index_t* index;
  size_t index_count, index_capacity;
  // Whole recording
  uint64_t frames, bytes_before_part;
  // Audio waiting for the next video frame
  int16_t* audio;
  size_t audio_frames, audio_capacity;
};

static bool se_avi_write(se_avi_writer_t* avi, const void* data, size_t size){
  if(avi->error)return false;
  if(size&&fwrite(data,1,size,avi->file)!=size){
    avi->error = "Could not write the video file (is the disk full?)";
    return false;
  }
  avi->file_size+=size;
  return true;
}

static uint32_t se_fourcc(const char* s){return (uint32_t)s[0]|((uint32_t)s[1]<<8)|((uint32_t)s[2]<<16)|((uint32_t)s[3]<<24);}

static bool se_avi_begin_part(se_avi_writer_t* avi){
  char path[4096+16];
  se_record_part_path(avi->path,avi->part,path,sizeof(path));
  avi->file = fopen(path,"wb");
  if(!avi->file){
    avi->error = "Could not create the video file";
    return false;
  }
  setvbuf(avi->file,NULL,_IOFBF,1<<20);
  avi->file_size = 0;
  avi->part_frames = avi->part_audio_frames = avi->max_chunk = 0;
  avi->index_count = 0;

  const se_avi_format_t* f = &avi->format;
  bool audio = f->audio_rate&&f->audio_channels;
  bool mjpeg = f->quality>0;
  uint32_t frame_bytes = mjpeg? (uint32_t)(f->width*f->height*3) : (uint32_t)((((uint32_t)f->width*3+3)&~3u)*f->height);
  uint8_t h[512];
  memset(h,0,sizeof(h));
  size_t o = 0;
  #define FOURCC(s) do{memcpy(h+o,s,4); o+=4;}while(0)
  #define U32(v) do{se_put32(h+o,(uint32_t)(v)); o+=4;}while(0)
  #define U16(v) do{se_put16(h+o,(uint32_t)(v)); o+=2;}while(0)
  FOURCC("RIFF"); avi->riff_size_pos = (long)o; U32(0); FOURCC("AVI ");
  FOURCC("LIST"); size_t hdrl_size = o; U32(0); FOURCC("hdrl");
  FOURCC("avih"); U32(56);
  U32((uint64_t)1000000*f->fps_den/f->fps_num);     // dwMicroSecPerFrame
  U32(0);                                          // dwMaxBytesPerSec
  U32(0);                                          // dwPaddingGranularity
  U32(0x10|0x100);                                 // AVIF_HASINDEX | AVIF_ISINTERLEAVED
  avi->avih_frames_pos = (long)o; U32(0);          // dwTotalFrames
  U32(0);                                          // dwInitialFrames
  U32(audio? 2 : 1);                               // dwStreams
  avi->avih_buffer_pos = (long)o; U32(frame_bytes);// dwSuggestedBufferSize
  U32(f->width); U32(f->height);
  U32(0); U32(0); U32(0); U32(0);
  // Video stream
  FOURCC("LIST"); size_t strl_size = o; U32(0); FOURCC("strl");
  FOURCC("strh"); U32(56);
  FOURCC("vids"); if(mjpeg)FOURCC("MJPG"); else U32(0);
  U32(0); U16(0); U16(0); U32(0);
  U32(f->fps_den); U32(f->fps_num); U32(0);       // dwScale, dwRate, dwStart
  avi->video_length_pos = (long)o; U32(0);         // dwLength
  avi->video_buffer_pos = (long)o; U32(frame_bytes);
  U32(0xffffffffu); U32(0);                        // dwQuality, dwSampleSize
  U16(0); U16(0); U16(f->width); U16(f->height);   // rcFrame
  FOURCC("strf"); U32(40);
  U32(40); U32(f->width); U32(f->height); U16(1); U16(24);
  if(mjpeg)FOURCC("MJPG"); else U32(0);            // BI_RGB
  U32(frame_bytes); U32(0); U32(0); U32(0); U32(0);
  se_put32(h+strl_size,(uint32_t)(o-strl_size-4));
  if(audio){
    uint32_t block = f->audio_channels*2;
    FOURCC("LIST"); size_t astrl_size = o; U32(0); FOURCC("strl");
    FOURCC("strh"); U32(56);
    FOURCC("auds"); U32(0);
    U32(0); U16(0); U16(0); U32(0);
    U32(block); U32(f->audio_rate*block); U32(0);  // dwScale, dwRate, dwStart
    avi->audio_length_pos = (long)o; U32(0);       // dwLength in sample frames
    U32(f->audio_rate*block/10);
    U32(0xffffffffu); U32(block);
    U16(0); U16(0); U16(0); U16(0);
    FOURCC("strf"); U32(18);
    U16(1); U16(f->audio_channels); U32(f->audio_rate); U32(f->audio_rate*block); U16(block); U16(16); U16(0);
    se_put32(h+astrl_size,(uint32_t)(o-astrl_size-4));
  }
  se_put32(h+hdrl_size,(uint32_t)(o-hdrl_size-4));
  avi->movi_list = (long)o;
  FOURCC("LIST"); avi->movi_size_pos = (long)o; U32(0); FOURCC("movi");
  #undef FOURCC
  #undef U32
  #undef U16
  return se_avi_write(avi,h,o);
}

static bool se_avi_add_index(se_avi_writer_t* avi, uint32_t id, uint32_t offset, uint32_t size){
  if(avi->index_count==avi->index_capacity){
    size_t capacity = avi->index_capacity? avi->index_capacity*2 : 4096;
    se_avi_index_t* index = (se_avi_index_t*)realloc(avi->index,capacity*sizeof(se_avi_index_t));
    if(!index){
      avi->error = "Not enough memory to record";
      return false;
    }
    avi->index = index;
    avi->index_capacity = capacity;
  }
  avi->index[avi->index_count++] = (se_avi_index_t){id,SE_AVIIF_KEYFRAME,offset,size};
  return true;
}

static bool se_avi_write_chunk(se_avi_writer_t* avi, const char* id, const void* data, size_t size){
  uint8_t header[8];
  memcpy(header,id,4);
  se_put32(header+4,(uint32_t)size);
  uint32_t offset = (uint32_t)(avi->file_size-(uint64_t)(avi->movi_list+8));
  if(!se_avi_add_index(avi,se_fourcc(id),offset,(uint32_t)size))return false;
  static const uint8_t pad = 0;
  if(size>avi->max_chunk)avi->max_chunk = (uint32_t)size;
  return se_avi_write(avi,header,8)&&se_avi_write(avi,data,size)&&((size&1)==0||se_avi_write(avi,&pad,1));
}

static bool se_avi_end_part(se_avi_writer_t* avi){
  if(!avi->file)return false;
  bool ok = !avi->error;
  if(ok){
    long idx1 = (long)avi->file_size;
    uint8_t header[8];
    memcpy(header,"idx1",4);
    se_put32(header+4,(uint32_t)(avi->index_count*16));
    ok = se_avi_write(avi,header,8);
    for(size_t i=0;ok&&i<avi->index_count;++i){
      uint8_t e[16];
      se_put32(e,avi->index[i].id);
      se_put32(e+4,avi->index[i].flags);
      se_put32(e+8,avi->index[i].offset);
      se_put32(e+12,avi->index[i].size);
      ok = se_avi_write(avi,e,16);
    }
    ok = ok&&fflush(avi->file)==0;
    ok = ok&&se_record_patch32(avi->file,avi->riff_size_pos,(uint32_t)(avi->file_size-8));
    ok = ok&&se_record_patch32(avi->file,avi->movi_size_pos,(uint32_t)(idx1-avi->movi_list-8));
    ok = ok&&se_record_patch32(avi->file,avi->avih_frames_pos,avi->part_frames);
    ok = ok&&se_record_patch32(avi->file,avi->video_length_pos,avi->part_frames);
    if(avi->max_chunk){
      ok = ok&&se_record_patch32(avi->file,avi->avih_buffer_pos,avi->max_chunk);
      ok = ok&&se_record_patch32(avi->file,avi->video_buffer_pos,avi->max_chunk);
    }
    if(avi->format.audio_rate&&avi->format.audio_channels){
      ok = ok&&se_record_patch32(avi->file,avi->audio_length_pos,avi->part_audio_frames);
    }
    if(!ok&&!avi->error)avi->error = "Could not write the video file (is the disk full?)";
  }
  if(fclose(avi->file)!=0&&ok){
    avi->error = "Could not write the video file (is the disk full?)";
    ok = false;
  }
  avi->file = NULL;
  avi->bytes_before_part+=avi->file_size;
  avi->file_size = 0;
  return ok;
}

se_avi_writer_t* se_avi_open(const char* path, const se_avi_format_t* format){
  if(!path||!format||format->width<=0||format->height<=0||!format->fps_num||!format->fps_den)return NULL;
  if(format->width>0xffff||format->height>0xffff)return NULL;
  se_avi_writer_t* avi = (se_avi_writer_t*)calloc(1,sizeof(se_avi_writer_t));
  if(!avi)return NULL;
  avi->format = *format;
  if(!avi->format.split_size)avi->format.split_size = SE_AVI_DEFAULT_SPLIT;
  snprintf(avi->path,sizeof(avi->path),"%s",path);
  avi->part = 1;
  if(!se_avi_begin_part(avi)){
    if(avi->file)fclose(avi->file);
    free(avi);
    return NULL;
  }
  return avi;
}

bool se_avi_add_audio(se_avi_writer_t* avi, const int16_t* samples, size_t frames){
  uint32_t channels = avi->format.audio_channels;
  if(avi->error)return false;
  if(!avi->format.audio_rate||!channels||!frames)return true;
  if(avi->audio_frames+frames>avi->audio_capacity){
    size_t capacity = avi->audio_capacity? avi->audio_capacity : 4096;
    while(capacity<avi->audio_frames+frames)capacity*=2;
    int16_t* audio = (int16_t*)realloc(avi->audio,capacity*channels*sizeof(int16_t));
    if(!audio){
      avi->error = "Not enough memory to record";
      return false;
    }
    avi->audio = audio;
    avi->audio_capacity = capacity;
  }
  memcpy(avi->audio+avi->audio_frames*channels,samples,frames*channels*sizeof(int16_t));
  avi->audio_frames+=frames;
  return true;
}

static bool se_avi_flush_audio(se_avi_writer_t* avi){
  if(!avi->audio_frames)return true;
  size_t bytes = avi->audio_frames*avi->format.audio_channels*sizeof(int16_t);
  // The samples are already little endian on every platform SkyEmu runs on
  bool ok = se_avi_write_chunk(avi,"01wb",avi->audio,bytes);
  if(ok)avi->part_audio_frames+=(uint32_t)avi->audio_frames;
  avi->audio_frames = 0;
  return ok;
}

bool se_avi_add_frame(se_avi_writer_t* avi, const uint8_t* data, size_t size){
  if(avi->error||!avi->file)return false;
  size_t audio_bytes = avi->audio_frames*avi->format.audio_channels*sizeof(int16_t);
  // Index entries, chunk headers and padding of this frame
  uint64_t needed = size+audio_bytes+64+(avi->index_count+2)*16;
  if(avi->part_frames&&avi->file_size+needed>avi->format.split_size){
    if(!se_avi_end_part(avi))return false;
    avi->part++;
    if(!se_avi_begin_part(avi))return false;
  }
  if(!se_avi_flush_audio(avi))return false;
  if(!se_avi_write_chunk(avi,avi->format.quality>0? "00dc" : "00db",data,size))return false;
  avi->part_frames++;
  avi->frames++;
  return true;
}

bool se_avi_close(se_avi_writer_t* avi){
  if(!avi)return false;
  bool ok = !avi->error;
  if(avi->file){
    if(ok)ok = se_avi_flush_audio(avi);
    ok = se_avi_end_part(avi)&&ok;
  }
  free(avi->index);
  free(avi->audio);
  free(avi);
  return ok;
}

uint64_t se_avi_frames(const se_avi_writer_t* avi){return avi->frames;}
uint64_t se_avi_bytes(const se_avi_writer_t* avi){return avi->bytes_before_part+avi->file_size;}
int se_avi_parts(const se_avi_writer_t* avi){return avi->part;}
const char* se_avi_error(const se_avi_writer_t* avi){return avi->error;}

///////////////////
// WAV           //
///////////////////
struct se_wav_writer_t{
  char path[4096];
  FILE* file;
  uint32_t rate, channels;
  int part;
  uint64_t part_bytes, frames;
  const char* error;
};

static bool se_wav_begin_part(se_wav_writer_t* wav){
  char path[4096+16];
  se_record_part_path(wav->path,wav->part,path,sizeof(path));
  wav->file = fopen(path,"wb");
  if(!wav->file){
    wav->error = "Could not create the audio file";
    return false;
  }
  setvbuf(wav->file,NULL,_IOFBF,1<<18);
  uint8_t h[44];
  uint32_t block = wav->channels*2;
  memcpy(h,"RIFF",4); se_put32(h+4,36); memcpy(h+8,"WAVE",4);
  memcpy(h+12,"fmt ",4); se_put32(h+16,16);
  se_put16(h+20,1); se_put16(h+22,wav->channels); se_put32(h+24,wav->rate);
  se_put32(h+28,wav->rate*block); se_put16(h+32,block); se_put16(h+34,16);
  memcpy(h+36,"data",4); se_put32(h+40,0);
  wav->part_bytes = 0;
  if(fwrite(h,1,44,wav->file)!=44){
    wav->error = "Could not write the audio file (is the disk full?)";
    return false;
  }
  return true;
}

static bool se_wav_end_part(se_wav_writer_t* wav){
  bool ok = !wav->error&&fflush(wav->file)==0;
  ok = ok&&se_record_patch32(wav->file,4,(uint32_t)(36+wav->part_bytes));
  ok = ok&&se_record_patch32(wav->file,40,(uint32_t)wav->part_bytes);
  ok = fclose(wav->file)==0&&ok;
  wav->file = NULL;
  if(!ok&&!wav->error)wav->error = "Could not write the audio file (is the disk full?)";
  return ok;
}

se_wav_writer_t* se_wav_open(const char* path, uint32_t rate, uint32_t channels){
  if(!path||!rate||!channels)return NULL;
  se_wav_writer_t* wav = (se_wav_writer_t*)calloc(1,sizeof(se_wav_writer_t));
  if(!wav)return NULL;
  snprintf(wav->path,sizeof(wav->path),"%s",path);
  wav->rate = rate;
  wav->channels = channels;
  wav->part = 1;
  if(!se_wav_begin_part(wav)){
    if(wav->file)fclose(wav->file);
    free(wav);
    return NULL;
  }
  return wav;
}

bool se_wav_add_audio(se_wav_writer_t* wav, const int16_t* samples, size_t frames){
  if(wav->error||!wav->file)return false;
  size_t bytes = frames*wav->channels*sizeof(int16_t);
  if(wav->part_bytes&&wav->part_bytes+bytes>SE_WAV_SPLIT){
    if(!se_wav_end_part(wav))return false;
    wav->part++;
    if(!se_wav_begin_part(wav))return false;
  }
  if(bytes&&fwrite(samples,1,bytes,wav->file)!=bytes){
    wav->error = "Could not write the audio file (is the disk full?)";
    return false;
  }
  wav->part_bytes+=bytes;
  wav->frames+=frames;
  return true;
}

bool se_wav_close(se_wav_writer_t* wav){
  if(!wav)return false;
  bool ok = !wav->error;
  if(wav->file)ok = se_wav_end_part(wav)&&ok;
  free(wav);
  return ok;
}

uint64_t se_wav_frames(const se_wav_writer_t* wav){return wav->frames;}
const char* se_wav_error(const se_wav_writer_t* wav){return wav->error;}

///////////////////
// Replay buffer //
///////////////////
bool se_replay_init(se_replay_buffer_t* replay, uint32_t capacity, int width, int height, int quality,
                    uint32_t audio_channels){
  se_replay_free(replay);
  if(!capacity)return false;
  replay->frames = (se_replay_frame_t*)calloc(capacity,sizeof(se_replay_frame_t));
  if(!replay->frames)return false;
  replay->capacity = capacity;
  replay->width = width;
  replay->height = height;
  replay->quality = quality;
  replay->audio_channels = audio_channels;
  return true;
}

void se_replay_free(se_replay_buffer_t* replay){
  for(uint32_t i=0;replay->frames&&i<replay->capacity;++i){
    se_record_buffer_free(&replay->frames[i].video);
    free(replay->frames[i].audio);
  }
  free(replay->frames);
  memset(replay,0,sizeof(*replay));
}

void se_replay_clear(se_replay_buffer_t* replay){
  replay->count = replay->next = 0;
}

bool se_replay_push(se_replay_buffer_t* replay, const uint8_t* data, size_t size, const int16_t* audio,
                    uint32_t audio_frames){
  if(!replay->capacity)return false;
  se_replay_frame_t* frame = replay->frames+replay->next;
  frame->video.size = 0;
  frame->audio_frames = 0;
  if(!se_record_buffer_append(&frame->video,data,size))return false;
  if(audio_frames&&replay->audio_channels){
    if(audio_frames>frame->audio_capacity){
      int16_t* a = (int16_t*)realloc(frame->audio,(size_t)audio_frames*replay->audio_channels*sizeof(int16_t));
      if(!a)return false;
      frame->audio = a;
      frame->audio_capacity = audio_frames;
    }
    memcpy(frame->audio,audio,(size_t)audio_frames*replay->audio_channels*sizeof(int16_t));
    frame->audio_frames = audio_frames;
  }
  replay->next = (replay->next+1)%replay->capacity;
  if(replay->count<replay->capacity)replay->count++;
  return true;
}

bool se_replay_save(const se_replay_buffer_t* replay, const char* path, const se_avi_format_t* format){
  if(!replay->count)return false;
  se_avi_writer_t* avi = se_avi_open(path,format);
  if(!avi)return false;
  uint32_t first = (replay->next+replay->capacity-replay->count)%replay->capacity;
  bool ok = true;
  for(uint32_t i=0;ok&&i<replay->count;++i){
    const se_replay_frame_t* frame = replay->frames+(first+i)%replay->capacity;
    ok = se_avi_add_audio(avi,frame->audio,frame->audio_frames)&&
         se_avi_add_frame(avi,frame->video.data,frame->video.size);
  }
  return se_avi_close(avi)&&ok;
}
