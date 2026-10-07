/*****************************************************************************
 *
 *   SkyEmu cheat finder, see se_cheat_finder.h
 *
**/
#include "se_cheat_finder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool se_search_compare_uses_value(se_search_compare_t compare){
  switch(compare){
    case SE_SEARCH_EQUAL:
    case SE_SEARCH_NOT_EQUAL:
    case SE_SEARCH_GREATER:
    case SE_SEARCH_LESS:
    case SE_SEARCH_INCREASED_BY:
    case SE_SEARCH_DECREASED_BY: return true;
    default: return false;
  }
}

uint32_t se_search_read(const uint8_t* memory, uint32_t offset, int value_size){
  const uint8_t* p = memory+offset;
  switch(value_size){
    case 1: return p[0];
    case 2: return (uint32_t)p[0]|((uint32_t)p[1]<<8);
    default: return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
  }
}

static uint32_t se_search_mask(int value_size){
  return value_size==1? 0xffu : value_size==2? 0xffffu : 0xffffffffu;
}

static int64_t se_search_signed(uint32_t value, int value_size){
  if(value_size==1)return (int8_t)value;
  if(value_size==2)return (int16_t)value;
  return (int32_t)value;
}

void se_search_reset(se_cheat_search_t* search){
  free(search->previous);
  free(search->candidates);
  memset(search,0,sizeof(*search));
}

bool se_search_active(const se_cheat_search_t* search){return search->candidates!=NULL;}

bool se_search_start(se_cheat_search_t* search, const se_search_region_t* regions, int num_regions,
                     const uint8_t* memory, int value_size, int alignment, bool is_signed){
  se_search_reset(search);
  if(num_regions<1||num_regions>SE_SEARCH_MAX_REGIONS)return false;
  if(value_size!=1&&value_size!=2&&value_size!=4)return false;
  if(alignment<1)alignment = 1;
  uint32_t memory_size = 0;
  for(int i=0;i<num_regions;++i){
    search->regions[i] = regions[i];
    memory_size+=regions[i].size;
  }
  if(memory_size==0)return false;
  search->num_regions = num_regions;
  search->memory_size = memory_size;
  search->value_size = value_size;
  search->alignment = alignment;
  search->is_signed = is_signed;
  search->previous = (uint8_t*)malloc(memory_size);
  search->candidates = (uint32_t*)calloc((memory_size+31)/32,sizeof(uint32_t));
  if(!search->previous||!search->candidates){
    se_search_reset(search);
    return false;
  }
  memcpy(search->previous,memory,memory_size);
  // Values may not cross from one region into the next
  uint32_t start = 0;
  for(int r=0;r<num_regions;++r){
    uint32_t size = regions[r].size;
    for(uint32_t o=0;o+value_size<=size;o+=alignment){
      uint32_t offset = start+o;
      search->candidates[offset/32] |= 1u<<(offset%32);
      search->num_candidates++;
    }
    start+=size;
  }
  return true;
}

static bool se_search_test(const se_cheat_search_t* search, uint32_t current, uint32_t previous,
                           se_search_compare_t compare, uint32_t value){
  int size = search->value_size;
  uint32_t mask = se_search_mask(size);
  value&=mask;
  int64_t cur_s = se_search_signed(current,size), prev_s = se_search_signed(previous,size), value_s = se_search_signed(value,size);
  switch(compare){
    case SE_SEARCH_EQUAL:        return current==value;
    case SE_SEARCH_NOT_EQUAL:    return current!=value;
    case SE_SEARCH_GREATER:      return search->is_signed? cur_s>value_s : current>value;
    case SE_SEARCH_LESS:         return search->is_signed? cur_s<value_s : current<value;
    case SE_SEARCH_CHANGED:      return current!=previous;
    case SE_SEARCH_UNCHANGED:    return current==previous;
    case SE_SEARCH_INCREASED:    return search->is_signed? cur_s>prev_s : current>previous;
    case SE_SEARCH_DECREASED:    return search->is_signed? cur_s<prev_s : current<previous;
    case SE_SEARCH_INCREASED_BY: return current!=previous&&((current-previous)&mask)==value;
    case SE_SEARCH_DECREASED_BY: return current!=previous&&((previous-current)&mask)==value;
    default: return false;
  }
}

uint32_t se_search_filter(se_cheat_search_t* search, const uint8_t* memory, se_search_compare_t compare, uint32_t value){
  if(!se_search_active(search))return 0;
  uint32_t words = (search->memory_size+31)/32;
  uint32_t count = 0;
  for(uint32_t w=0;w<words;++w){
    uint32_t bits = search->candidates[w];
    uint32_t keep = bits;
    while(bits){
      int b = 0;
      while(!(bits&(1u<<b)))++b;
      bits&=~(1u<<b);
      uint32_t offset = w*32+b;
      uint32_t current = se_search_read(memory,offset,search->value_size);
      uint32_t previous = se_search_read(search->previous,offset,search->value_size);
      if(se_search_test(search,current,previous,compare,value))count++;
      else keep&=~(1u<<b);
    }
    search->candidates[w] = keep;
  }
  memcpy(search->previous,memory,search->memory_size);
  search->num_candidates = count;
  search->searches++;
  return count;
}

uint32_t se_search_results(const se_cheat_search_t* search, uint32_t first_result, uint32_t* offsets, uint32_t max){
  if(!se_search_active(search)||max==0)return 0;
  uint32_t words = (search->memory_size+31)/32;
  uint32_t index = 0, written = 0;
  for(uint32_t w=0;w<words&&written<max;++w){
    uint32_t bits = search->candidates[w];
    if(!bits)continue;
    // Skip whole words of results before the first one requested
    uint32_t pop = 0;
    for(uint32_t b = bits;b;b&=b-1)pop++;
    if(index+pop<=first_result){
      index+=pop;
      continue;
    }
    for(int b=0;b<32&&written<max;++b){
      if(!(bits&(1u<<b)))continue;
      if(index++>=first_result)offsets[written++] = w*32+b;
    }
  }
  return written;
}

uint32_t se_search_address(const se_cheat_search_t* search, uint32_t offset){
  uint32_t start = 0;
  for(int r=0;r<search->num_regions;++r){
    if(offset<start+search->regions[r].size)return search->regions[r].address+(offset-start);
    start+=search->regions[r].size;
  }
  return 0;
}

// Action Replay v3 uses TEA with these keys (thanks fleroviux for the decryption)
#define SE_ARV3_S0 0x7AA9648Fu
#define SE_ARV3_S1 0x7FAE6994u
#define SE_ARV3_S2 0xC0EFAAD5u
#define SE_ARV3_S3 0x42712C57u
#define SE_ARV3_DELTA 0x9E3779B9u

uint64_t se_gba_decrypt_arv3(uint64_t code){
  uint32_t l = (uint32_t)(code>>32), r = (uint32_t)code;
  uint32_t sum = SE_ARV3_DELTA<<5;
  for(int i=0;i<32;++i){
    r -= ((l<<4)+SE_ARV3_S2)^(l+sum)^((l>>5)+SE_ARV3_S3);
    l -= ((r<<4)+SE_ARV3_S0)^(r+sum)^((r>>5)+SE_ARV3_S1);
    sum -= SE_ARV3_DELTA;
  }
  return ((uint64_t)l<<32)|r;
}

uint64_t se_gba_encrypt_arv3(uint64_t code){
  uint32_t l = (uint32_t)(code>>32), r = (uint32_t)code;
  uint32_t sum = SE_ARV3_DELTA;
  for(int i=0;i<32;++i){
    l += ((r<<4)+SE_ARV3_S0)^(r+sum)^((r>>5)+SE_ARV3_S1);
    r += ((l<<4)+SE_ARV3_S2)^(l+sum)^((l>>5)+SE_ARV3_S3);
    sum += SE_ARV3_DELTA;
  }
  return ((uint64_t)l<<32)|r;
}

// GBA: 00aaaaaa 000000vv (8 bit), 02aaaaaa 0000vvvv (16 bit), 04aaaaaa vvvvvvvv (32 bit), where
// aaaaaa holds address bits 24-27 in bits 20-23 and address bits 0-19 below, encrypted
static int se_cheat_make_gba(uint32_t address, uint32_t value, int value_size, uint32_t* words, int max_words){
  if(address>=0x10000000u||(address&0x00F00000u))return 0;
  if(address%value_size)return 0;
  // A decrypted right half of 0x001DC0DE marks a code to skip, write the halves separately
  if(value_size==4&&value==0x001DC0DEu){
    int n = se_cheat_make_gba(address,value&0xffff,2,words,max_words);
    if(!n)return 0;
    int m = se_cheat_make_gba(address+2,value>>16,2,words+n,max_words-n);
    return m? n+m : 0;
  }
  if(max_words<2)return 0;
  uint32_t type = value_size==1? 0x00 : value_size==2? 0x02 : 0x04;
  uint32_t left = (type<<24)|((address>>4)&0x00F00000u)|(address&0x000FFFFFu);
  uint32_t right = value&se_search_mask(value_size);
  if(left==0)return 0;
  uint64_t code = se_gba_encrypt_arv3(((uint64_t)left<<32)|right);
  words[0] = (uint32_t)(code>>32);
  words[1] = (uint32_t)code;
  return 2;
}

// GB / GBC GameShark: bbvvllhh, one code per byte, for A000-DFFF. Bits 16-23 of the address
// are the cartridge RAM bank used for A000-BFFF.
static int se_cheat_make_gb(uint32_t address, uint32_t value, int value_size, uint32_t* words, int max_words){
  uint32_t bank = (address>>16)&0xff;
  uint32_t addr = address&0xffff;
  if(address>>24)return 0;
  if(max_words<value_size)return 0;
  for(int i=0;i<value_size;++i){
    uint32_t a = addr+i;
    if(a<0xA000||a>0xDFFF)return 0;
    if(a>=0xC000)bank = 0x01;
    uint32_t byte = (value>>(8*i))&0xff;
    words[i] = (bank<<24)|(byte<<16)|((a&0xff)<<8)|(a>>8);
  }
  return value_size;
}

// NDS Action Replay: 0aaaaaaa vvvvvvvv (32 bit), 1aaaaaaa 0000vvvv (16 bit), 2aaaaaaa 000000vv (8 bit)
static int se_cheat_make_nds(uint32_t address, uint32_t value, int value_size, uint32_t* words, int max_words){
  if(address>=0x10000000u||address%value_size||max_words<2)return 0;
  uint32_t type = value_size==4? 0x0 : value_size==2? 0x1 : 0x2;
  words[0] = (type<<28)|address;
  words[1] = value&se_search_mask(value_size);
  return 2;
}

int se_cheat_make_code(se_cheat_system_t system, uint32_t address, uint32_t value, int value_size,
                       uint32_t* words, int max_words){
  if(value_size!=1&&value_size!=2&&value_size!=4)return 0;
  switch(system){
    case SE_CHEAT_SYSTEM_GB:  return se_cheat_make_gb(address,value,value_size,words,max_words);
    case SE_CHEAT_SYSTEM_GBA: return se_cheat_make_gba(address,value,value_size,words,max_words);
    case SE_CHEAT_SYSTEM_NDS: return se_cheat_make_nds(address,value,value_size,words,max_words);
    default: return 0;
  }
}

void se_cheat_format_code(const uint32_t* words, int num_words, char* out, size_t out_size){
  if(!out_size)return;
  out[0] = 0;
  size_t off = 0;
  for(int i=0;i<num_words&&off<out_size;++i){
    const char* separator = i+1==num_words? "" : (i%2)? "\n" : " ";
    int n = snprintf(out+off,out_size-off,"%08X%s",words[i],separator);
    if(n<0)break;
    off+=(size_t)n;
  }
}
