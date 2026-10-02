/*****************************************************************************
 *
 *   SkyEmu ROM patches, see se_patch.h
 *
**/
#include "se_patch.h"

#include <stdlib.h>
#include <string.h>

const char* se_patch_extensions[SE_PATCH_NUM_EXTENSIONS] = {".ips",".ups",".bps"};

// Patched ROMs larger than this are rejected, NDS ROMs top out at 512 MiB
#define SE_PATCH_MAX_TARGET_SIZE (1024u*1024u*1024u)

static const char* se_patch_error_damaged   = "The patch is damaged or incomplete";
static const char* se_patch_error_checksum  = "The patch file is damaged (checksum mismatch)";
static const char* se_patch_error_wrong_rom = "The patch was made for a different ROM";
static const char* se_patch_error_result    = "The patched ROM does not match the patch's checksum";
static const char* se_patch_error_memory    = "Not enough memory to apply the patch";
static const char* se_patch_error_format    = "Unknown patch format";
static const char* se_patch_error_too_large = "The patched ROM would be too large";

uint32_t se_patch_crc32(const uint8_t* data, size_t size){
  static uint32_t table[256];
  static bool table_ready = false;
  if(!table_ready){
    for(uint32_t i=0;i<256;++i){
      uint32_t c = i;
      for(int k=0;k<8;++k)c = (c&1)? 0xEDB88320u^(c>>1) : c>>1;
      table[i] = c;
    }
    table_ready = true;
  }
  uint32_t crc = 0xFFFFFFFFu;
  for(size_t i=0;i<size;++i)crc = table[(crc^data[i])&0xff]^(crc>>8);
  return crc^0xFFFFFFFFu;
}

se_patch_format_t se_patch_detect(const uint8_t* patch, size_t patch_size){
  if(!patch)return SE_PATCH_UNKNOWN;
  if(patch_size>=8&&memcmp(patch,"PATCH",5)==0)return SE_PATCH_IPS;
  if(patch_size>=16&&memcmp(patch,"UPS1",4)==0)return SE_PATCH_UPS;
  if(patch_size>=16&&memcmp(patch,"BPS1",4)==0)return SE_PATCH_BPS;
  return SE_PATCH_UNKNOWN;
}

const char* se_patch_format_name(se_patch_format_t format){
  switch(format){
    case SE_PATCH_IPS: return "IPS";
    case SE_PATCH_UPS: return "UPS";
    case SE_PATCH_BPS: return "BPS";
    default: return "Unknown";
  }
}

static uint32_t se_patch_read_le32(const uint8_t* p){
  return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}

// Variable length number used by UPS and BPS (7 bits per byte, last byte has bit 7 set)
static bool se_patch_read_number(const uint8_t* patch, size_t end, size_t* pos, uint64_t* value){
  uint64_t data = 0, shift = 1;
  for(int i=0;i<10;++i){
    if(*pos>=end)return false;
    uint8_t x = patch[(*pos)++];
    data += (uint64_t)(x&0x7f)*shift;
    if(x&0x80){
      *value = data;
      return true;
    }
    shift <<= 7;
    data += shift;
  }
  return false;
}

static bool se_patch_apply_ips(const uint8_t* rom, size_t rom_size, const uint8_t* patch, size_t patch_size,
                               uint8_t** out, size_t* out_size, const char** error){
  // First pass: validate the records and find the size of the patched ROM
  size_t pos = 5, size = rom_size;
  bool found_eof = false;
  while(pos+3<=patch_size){
    uint32_t offset = ((uint32_t)patch[pos]<<16)|((uint32_t)patch[pos+1]<<8)|patch[pos+2];
    pos+=3;
    if(offset==0x454F46){ // "EOF"
      found_eof = true;
      break;
    }
    if(pos+2>patch_size){*error = se_patch_error_damaged; return false;}
    uint32_t length = ((uint32_t)patch[pos]<<8)|patch[pos+1];
    pos+=2;
    if(length==0){ // Run of one repeated byte
      if(pos+3>patch_size){*error = se_patch_error_damaged; return false;}
      length = ((uint32_t)patch[pos]<<8)|patch[pos+1];
      pos+=3;
    }else{
      if(pos+length>patch_size){*error = se_patch_error_damaged; return false;}
      pos+=length;
    }
    if((size_t)offset+length>size)size = (size_t)offset+length;
  }
  if(!found_eof){*error = se_patch_error_damaged; return false;}
  // Optional 3 byte size after EOF truncates the ROM (Lunar IPS extension)
  bool truncate = patch_size-pos==3;
  size_t truncate_size = 0;
  if(truncate)truncate_size = ((size_t)patch[pos]<<16)|((size_t)patch[pos+1]<<8)|patch[pos+2];

  uint8_t* data = (uint8_t*)calloc(size? size : 1,1);
  if(!data){*error = se_patch_error_memory; return false;}
  memcpy(data,rom,rom_size);
  pos = 5;
  while(pos+3<=patch_size){
    uint32_t offset = ((uint32_t)patch[pos]<<16)|((uint32_t)patch[pos+1]<<8)|patch[pos+2];
    pos+=3;
    if(offset==0x454F46)break;
    uint32_t length = ((uint32_t)patch[pos]<<8)|patch[pos+1];
    pos+=2;
    if(length==0){
      length = ((uint32_t)patch[pos]<<8)|patch[pos+1];
      memset(data+offset,patch[pos+2],length);
      pos+=3;
    }else{
      memcpy(data+offset,patch+pos,length);
      pos+=length;
    }
  }
  if(truncate&&truncate_size<size)size = truncate_size;
  *out = data;
  *out_size = size;
  return true;
}

// Common header and footer checks of UPS and BPS. footer_start is where the three CRCs begin.
static bool se_patch_check_footer(const uint8_t* rom, size_t rom_size, const uint8_t* patch, size_t patch_size,
                                  uint64_t source_size, uint32_t* target_crc, const char** error){
  size_t footer = patch_size-12;
  uint32_t source_crc = se_patch_read_le32(patch+footer);
  *target_crc = se_patch_read_le32(patch+footer+4);
  uint32_t patch_crc = se_patch_read_le32(patch+footer+8);
  if(se_patch_crc32(patch,patch_size-4)!=patch_crc){*error = se_patch_error_checksum; return false;}
  if(source_size!=rom_size||se_patch_crc32(rom,rom_size)!=source_crc){*error = se_patch_error_wrong_rom; return false;}
  return true;
}

static bool se_patch_apply_ups(const uint8_t* rom, size_t rom_size, const uint8_t* patch, size_t patch_size,
                               uint8_t** out, size_t* out_size, const char** error){
  size_t end = patch_size-12, pos = 4;
  uint64_t source_size, target_size;
  if(!se_patch_read_number(patch,end,&pos,&source_size)||!se_patch_read_number(patch,end,&pos,&target_size)){
    *error = se_patch_error_damaged;
    return false;
  }
  uint32_t target_crc;
  if(!se_patch_check_footer(rom,rom_size,patch,patch_size,source_size,&target_crc,error))return false;
  if(target_size>SE_PATCH_MAX_TARGET_SIZE){*error = se_patch_error_too_large; return false;}
  uint8_t* data = (uint8_t*)calloc(target_size? target_size : 1,1);
  if(!data){*error = se_patch_error_memory; return false;}
  memcpy(data,rom,rom_size<target_size? rom_size : (size_t)target_size);
  // Hunks: skip unchanged bytes, then XOR bytes up to and including a zero byte
  uint64_t offset = 0;
  while(pos<end){
    uint64_t skip;
    if(!se_patch_read_number(patch,end,&pos,&skip)){free(data); *error = se_patch_error_damaged; return false;}
    offset+=skip;
    for(;;){
      if(pos>=end){free(data); *error = se_patch_error_damaged; return false;}
      uint8_t x = patch[pos++];
      if(offset<target_size)data[offset]^=x;
      offset++;
      if(x==0)break;
    }
  }
  if(se_patch_crc32(data,(size_t)target_size)!=target_crc){free(data); *error = se_patch_error_result; return false;}
  *out = data;
  *out_size = (size_t)target_size;
  return true;
}

static bool se_patch_apply_bps(const uint8_t* rom, size_t rom_size, const uint8_t* patch, size_t patch_size,
                               uint8_t** out, size_t* out_size, const char** error){
  size_t end = patch_size-12, pos = 4;
  uint64_t source_size, target_size, metadata_size;
  if(!se_patch_read_number(patch,end,&pos,&source_size)||!se_patch_read_number(patch,end,&pos,&target_size)||
     !se_patch_read_number(patch,end,&pos,&metadata_size)||metadata_size>end-pos){
    *error = se_patch_error_damaged;
    return false;
  }
  pos+=(size_t)metadata_size;
  uint32_t target_crc;
  if(!se_patch_check_footer(rom,rom_size,patch,patch_size,source_size,&target_crc,error))return false;
  if(target_size>SE_PATCH_MAX_TARGET_SIZE){*error = se_patch_error_too_large; return false;}
  uint8_t* data = (uint8_t*)malloc(target_size? target_size : 1);
  if(!data){*error = se_patch_error_memory; return false;}
  size_t out_pos = 0;
  int64_t source_rel = 0, target_rel = 0;
  while(pos<end){
    uint64_t command;
    if(!se_patch_read_number(patch,end,&pos,&command))goto damaged;
    uint64_t length = (command>>2)+1;
    if(length>target_size-out_pos)goto damaged;
    switch(command&3){
      case 0: // SourceRead: copy from the same position of the ROM
        if(out_pos+length>rom_size)goto damaged;
        memcpy(data+out_pos,rom+out_pos,(size_t)length);
        out_pos+=(size_t)length;
        break;
      case 1: // TargetRead: bytes stored in the patch
        if(length>end-pos)goto damaged;
        memcpy(data+out_pos,patch+pos,(size_t)length);
        pos+=(size_t)length;
        out_pos+=(size_t)length;
        break;
      case 2:
      case 3:{ // SourceCopy / TargetCopy: copy from a relative position of the ROM or the output
        uint64_t encoded;
        if(!se_patch_read_number(patch,end,&pos,&encoded))goto damaged;
        int64_t delta = (int64_t)(encoded>>1);
        if(encoded&1)delta = -delta;
        if((command&3)==2){
          source_rel+=delta;
          if(source_rel<0||(uint64_t)source_rel+length>rom_size)goto damaged;
          memcpy(data+out_pos,rom+source_rel,(size_t)length);
          source_rel+=(int64_t)length;
          out_pos+=(size_t)length;
        }else{
          target_rel+=delta;
          if(target_rel<0)goto damaged;
          // Byte by byte, the source may overlap the bytes being written (repeating patterns)
          for(uint64_t i=0;i<length;++i){
            if((size_t)target_rel>=out_pos)goto damaged;
            data[out_pos++] = data[target_rel++];
          }
        }
      }break;
    }
  }
  if(out_pos!=target_size)goto damaged;
  if(se_patch_crc32(data,(size_t)target_size)!=target_crc){free(data); *error = se_patch_error_result; return false;}
  *out = data;
  *out_size = (size_t)target_size;
  return true;
damaged:
  free(data);
  *error = se_patch_error_damaged;
  return false;
}

bool se_patch_apply(const uint8_t* rom, size_t rom_size, const uint8_t* patch, size_t patch_size,
                    uint8_t** out, size_t* out_size, const char** error){
  const char* unused_error;
  if(!error)error = &unused_error;
  *out = NULL;
  *out_size = 0;
  *error = NULL;
  if(!rom&&rom_size){*error = se_patch_error_wrong_rom; return false;}
  switch(se_patch_detect(patch,patch_size)){
    case SE_PATCH_IPS: return se_patch_apply_ips(rom,rom_size,patch,patch_size,out,out_size,error);
    case SE_PATCH_UPS: return se_patch_apply_ups(rom,rom_size,patch,patch_size,out,out_size,error);
    case SE_PATCH_BPS: return se_patch_apply_bps(rom,rom_size,patch,patch_size,out,out_size,error);
    default: *error = se_patch_error_format; return false;
  }
}
