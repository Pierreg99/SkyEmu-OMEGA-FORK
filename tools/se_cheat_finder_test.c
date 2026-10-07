/*****************************************************************************
 *
 *   Unit tests of the cheat finder (src/se_cheat_finder.c)
 *
 *   cc -O2 -Wall -Wextra -Isrc tools/se_cheat_finder_test.c src/se_cheat_finder.c -o se_cheat_finder_test
 *
 *   The generated codes are run through copies of the code decoding of SkyEmu's cheat
 *   engines (gba.h, gb.h, nds.h) to check they write the expected bytes.
 *
**/
#include "se_cheat_finder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, ...) do{ if(!(cond)){ failures++; printf("FAIL %s:%d: ",__FILE__,__LINE__); printf(__VA_ARGS__); printf("\n"); } }while(0)

static uint32_t rng_state = 0x12345678;
static uint32_t rng(void){
  rng_state ^= rng_state<<13; rng_state ^= rng_state>>17; rng_state ^= rng_state<<5;
  return rng_state;
}

// Copy of gba_decrypt_arv3() in gba.h
static uint64_t gba_h_decrypt_arv3(uint64_t code){
  const uint32_t S0 = 0x7AA9648F;
  const uint32_t S1 = 0x7FAE6994;
  const uint32_t S2 = 0xC0EFAAD5;
  const uint32_t S3 = 0x42712C57;
  uint32_t l = code >> 32;
  uint32_t r = code & 0xFFFFFFFF;
  uint32_t tmp = 0x9E3779B9 << 5;
  for (int i = 0; i < 32; i++) {
    r -= ((l << 4) + S2) ^ (l + tmp) ^ ((l >> 5) + S3);
    l -= ((r << 4) + S0) ^ (r + tmp) ^ ((r >> 5) + S1);
    tmp -= 0x9E3779B9;
  }
  return ((uint64_t)l << 32) | r;
}

// Simulated console memory written by the decoders below
static uint8_t sim[0x10000];
static uint32_t sim_base;
static void sim_store(uint32_t address, uint32_t value, int size){
  for(int i=0;i<size;++i){
    uint32_t a = address+i-sim_base;
    if(a<sizeof(sim))sim[a] = (value>>(8*i))&0xff;
  }
}

// Decoding of the 00, 02 and 04 codes of gba_run_ar_cheat() in gba.h
static bool gba_h_run(const uint32_t* buffer, int size){
  if(size%2)return false;
  for(int i=0;i<size;i+=2){
    uint64_t code = gba_h_decrypt_arv3(buffer[i+1]|((uint64_t)buffer[i]<<32));
    uint32_t left = code>>32, right = code&0xFFFFFFFF;
    if(right==0x1DC0DE)continue;
    if(left==0)return false;
    uint8_t current_code = (left>>24)&0xFF;
    uint32_t address = ((left<<4)&0x0F000000)|(left&0x000FFFFF);
    switch(current_code){
      case 0x00: sim_store(address+(right>>8),right&0xFF,1); break;
      case 0x02: sim_store(address+(right>>16)*2,right&0xFFFF,2); break;
      case 0x04: sim_store(address,right,4); break;
      default: return false;
    }
  }
  return true;
}

// Decoding of sb_run_ar_cheat() in gb.h, also returns the bank of the last code
static bool gb_h_run(const uint32_t* buffer, int size, uint8_t* last_bank){
  for(int i=0;i<size;i++){
    uint32_t cheat = buffer[i];
    uint16_t addr = cheat&0xffff;
    addr = addr>>8|addr<<8;
    if(addr<0xA000||addr>0xDFFF)return false;
    sim_store(addr,(cheat>>16)&0xff,1);
    *last_bank = cheat>>24;
  }
  return true;
}

// Decoding of the 0, 1 and 2 codes of nds_run_ar_cheat() in nds.h
static bool nds_h_run(const uint32_t* buffer, int size){
  if(size%2)return false;
  for(int i=0;i<size;i+=2){
    uint32_t address = buffer[i]&0x0fffffff;
    switch(buffer[i]>>28){
      case 0x0: sim_store(address,buffer[i+1],4); break;
      case 0x1: sim_store(address,buffer[i+1]&0xffff,2); break;
      case 0x2: sim_store(address,buffer[i+1]&0xff,1); break;
      default: return false;
    }
  }
  return true;
}

static uint32_t sim_read(uint32_t address, int size){
  uint32_t v = 0;
  for(int i=0;i<size;++i)v|=(uint32_t)sim[address+i-sim_base]<<(8*i);
  return v;
}

static void test_arv3(void){
  for(int i=0;i<20000;++i){
    uint64_t code = ((uint64_t)rng()<<32)|rng();
    uint64_t encrypted = se_gba_encrypt_arv3(code);
    CHECK(se_gba_decrypt_arv3(encrypted)==code,"ARv3 round trip of %016llx",(unsigned long long)code);
    CHECK(gba_h_decrypt_arv3(encrypted)==code,"gba.h decrypts %016llx",(unsigned long long)code);
    CHECK(se_gba_decrypt_arv3(code)==gba_h_decrypt_arv3(code),"same decryption as gba.h");
  }
}

static void test_codes(void){
  static const int sizes[3] = {1,2,4};
  uint32_t words[16];
  char text[256];
  // GBA: EWRAM and IWRAM
  static const uint32_t gba_bases[2] = {0x02000000,0x03000000};
  for(int i=0;i<3000;++i){
    int size = sizes[rng()%3];
    sim_base = gba_bases[rng()%2];
    uint32_t address = sim_base+((rng()%0xff00)&~(uint32_t)(size-1));
    uint32_t value = rng();
    if(i==0){size = 4; value = 0x001DC0DE;}
    memset(sim,0xAA,sizeof(sim));
    int n = se_cheat_make_code(SE_CHEAT_SYSTEM_GBA,address,value,size,words,16);
    CHECK(n==2||(i==0&&n==4),"GBA code size %d",n);
    CHECK(gba_h_run(words,n),"GBA code runs");
    uint32_t mask = size==4? 0xffffffffu : (1u<<(8*size))-1;
    CHECK(sim_read(address,size)==(value&mask),"GBA %d byte write of %08x at %08x gave %08x",size,value&mask,address,sim_read(address,size));
    if(address-sim_base>=1)CHECK(sim[address-sim_base-1]==0xAA,"GBA write stays in place");
    if(address-sim_base+size<sizeof(sim))CHECK(sim[address-sim_base+size]==0xAA,"GBA write stays in place");
  }
  CHECK(se_cheat_make_code(SE_CHEAT_SYSTEM_GBA,0x02000001,1,2,words,16)==0,"GBA unaligned 16 bit refused");
  CHECK(se_cheat_make_code(SE_CHEAT_SYSTEM_GBA,0x02100000,1,1,words,16)==0,"GBA address bits 20-23 refused");
  CHECK(se_cheat_make_code(SE_CHEAT_SYSTEM_GBA,0x12000000,1,1,words,16)==0,"GBA address above 0x0FFFFFFF refused");

  // GB: cartridge RAM with its bank and work RAM
  sim_base = 0;
  for(int i=0;i<3000;++i){
    int size = sizes[rng()%3];
    uint32_t addr = 0xA000+rng()%(0x4000-size+1);
    uint32_t bank = rng()%16;
    uint32_t value = rng();
    memset(sim,0xAA,sizeof(sim));
    int n = se_cheat_make_code(SE_CHEAT_SYSTEM_GB,(bank<<16)|addr,value,size,words,16);
    CHECK(n==size,"GB one code per byte");
    uint8_t last_bank = 0;
    CHECK(gb_h_run(words,n,&last_bank),"GB code runs");
    uint32_t mask = size==4? 0xffffffffu : (1u<<(8*size))-1;
    CHECK(sim_read(addr,size)==(value&mask),"GB %d byte write at %04x",size,addr);
    uint32_t last = addr+size-1;
    CHECK(last_bank==(last>=0xC000? 1 : bank),"GB bank %u for %04x",last_bank,last);
  }
  CHECK(se_cheat_make_code(SE_CHEAT_SYSTEM_GB,0xFF80,1,1,words,16)==0,"GB HRAM refused");
  CHECK(se_cheat_make_code(SE_CHEAT_SYSTEM_GB,0xDFFF,1,2,words,16)==0,"GB write past DFFF refused");
  se_cheat_make_code(SE_CHEAT_SYSTEM_GB,0xC123,0x63,1,words,16);
  CHECK(words[0]==0x016323C1,"GB GameShark layout %08X",words[0]);

  // NDS: main RAM
  sim_base = 0x02000000;
  for(int i=0;i<3000;++i){
    int size = sizes[rng()%3];
    uint32_t address = sim_base+((rng()%0xff00)&~(uint32_t)(size-1));
    uint32_t value = rng();
    memset(sim,0xAA,sizeof(sim));
    int n = se_cheat_make_code(SE_CHEAT_SYSTEM_NDS,address,value,size,words,16);
    CHECK(n==2,"NDS code size");
    CHECK(nds_h_run(words,n),"NDS code runs");
    uint32_t mask = size==4? 0xffffffffu : (1u<<(8*size))-1;
    CHECK(sim_read(address,size)==(value&mask),"NDS %d byte write at %08x",size,address);
  }
  se_cheat_make_code(SE_CHEAT_SYSTEM_NDS,0x02123456,0xBEEF,2,words,16);
  CHECK(words[0]==0x12123456&&words[1]==0x0000BEEF,"NDS 16 bit layout");

  uint32_t three[3] = {0x01020304,0xAABBCCDD,0x11223344};
  se_cheat_format_code(three,3,text,sizeof(text));
  CHECK(strcmp(text,"01020304 AABBCCDD\n11223344")==0,"format \"%s\"",text);
  se_cheat_format_code(three,3,text,10);
  CHECK(strlen(text)<10,"format truncates");
}

static void test_search(void){
  se_cheat_search_t search = {0};
  se_search_region_t regions[2] = {{0x02000000,64},{0x03000000,32}};
  uint8_t memory[96];
  memset(memory,0,sizeof(memory));
  // 8 bit: one value that goes 5 -> 4 -> 4 -> 9
  memory[10] = 5;
  CHECK(se_search_start(&search,regions,2,memory,1,1,false),"start");
  CHECK(search.num_candidates==96,"all bytes are candidates (%u)",search.num_candidates);
  CHECK(se_search_filter(&search,memory,SE_SEARCH_EQUAL,5)==1,"equal 5");
  memory[10] = 4; memory[20] = 7;
  CHECK(se_search_filter(&search,memory,SE_SEARCH_DECREASED,0)==1,"decreased");
  CHECK(se_search_filter(&search,memory,SE_SEARCH_UNCHANGED,0)==1,"unchanged");
  memory[10] = 9;
  CHECK(se_search_filter(&search,memory,SE_SEARCH_INCREASED_BY,5)==1,"increased by 5");
  uint32_t offset = 0;
  CHECK(se_search_results(&search,0,&offset,1)==1&&offset==10,"result offset %u",offset);
  CHECK(se_search_address(&search,offset)==0x0200000A,"address");
  CHECK(se_search_address(&search,70)==0x03000006,"address in the second region");
  CHECK(search.searches==4,"search count");

  // Changed / not equal / greater / less from a fresh start
  CHECK(se_search_start(&search,regions,2,memory,1,1,false),"restart");
  memory[3] = 200;
  CHECK(se_search_filter(&search,memory,SE_SEARCH_CHANGED,0)==1,"changed");
  CHECK(se_search_start(&search,regions,2,memory,1,1,false),"restart");
  CHECK(se_search_filter(&search,memory,SE_SEARCH_GREATER,8)==2,"greater than 8 (9 and 200)");
  CHECK(se_search_filter(&search,memory,SE_SEARCH_LESS,100)==1,"less than 100");
  CHECK(se_search_start(&search,regions,2,memory,1,1,true),"restart signed");
  CHECK(se_search_filter(&search,memory,SE_SEARCH_LESS,0)==1,"signed: 200 is -56");
  CHECK(se_search_start(&search,regions,2,memory,1,1,false),"restart");
  CHECK(se_search_filter(&search,memory,SE_SEARCH_NOT_EQUAL,0)==3,"not equal 0");

  // 16 bit aligned values never cross into the next region
  memset(memory,0,sizeof(memory));
  CHECK(se_search_start(&search,regions,2,memory,2,2,false),"start 16 bit");
  CHECK(search.num_candidates==32+16,"16 bit candidates %u",search.num_candidates);
  memory[62] = 0x34; memory[63] = 0x12;
  CHECK(se_search_filter(&search,memory,SE_SEARCH_EQUAL,0x1234)==1,"16 bit equal");
  // 32 bit unaligned (alignment 1) stops 3 bytes before each region end
  CHECK(se_search_start(&search,regions,2,memory,4,1,false),"start 32 bit unaligned");
  CHECK(search.num_candidates==(64-3)+(32-3),"32 bit candidates %u",search.num_candidates);
  // Decreased by wraps around like the console does
  memset(memory,0,sizeof(memory));
  CHECK(se_search_start(&search,regions,2,memory,1,1,false),"start");
  memory[5] = 0xFF;
  CHECK(se_search_filter(&search,memory,SE_SEARCH_DECREASED_BY,1)==1,"0 -> 255 decreased by 1");
  CHECK(se_search_filter(&search,memory,SE_SEARCH_DECREASED_BY,1)==0,"unchanged is not decreased by 1");

  // Results paging over many candidates
  static uint8_t big[100000];
  se_search_region_t big_region = {0x02000000,sizeof(big)};
  CHECK(se_search_start(&search,&big_region,1,big,1,1,false),"start big");
  for(int i=0;i<1000;++i)big[i*97] = 1;
  CHECK(se_search_filter(&search,big,SE_SEARCH_CHANGED,0)==1000,"1000 changed");
  uint32_t page[64];
  uint32_t n = se_search_results(&search,990,page,64);
  CHECK(n==10&&page[0]==990*97&&page[9]==999*97,"last page");
  n = se_search_results(&search,0,page,64);
  CHECK(n==64&&page[63]==63*97,"first page");
  CHECK(se_search_results(&search,1000,page,64)==0,"past the end");

  // Random cross-check against a straightforward implementation
  static uint8_t a[4096], b[4096];
  for(int round=0;round<200;++round){
    int size = 1<<(rng()%3);
    int alignment = rng()%2? size : 1;
    bool is_signed = rng()%2;
    for(size_t i=0;i<sizeof(a);++i)a[i] = rng()%4==0? rng() : 0;
    se_search_region_t r = {0x02000000,sizeof(a)};
    se_search_start(&search,&r,1,a,size,alignment,is_signed);
    memcpy(b,a,sizeof(a));
    for(size_t i=0;i<sizeof(b);++i)if(rng()%3==0)b[i]+=rng()%5-2;
    se_search_compare_t compare = (se_search_compare_t)(rng()%SE_SEARCH_NUM_COMPARES);
    uint32_t value = rng()%3;
    uint32_t expected = 0;
    uint32_t mask = size==4? 0xffffffffu : (1u<<(8*size))-1;
    for(uint32_t o=0;o+size<=sizeof(a);o+=alignment){
      uint32_t cur = se_search_read(b,o,size), prev = se_search_read(a,o,size);
      int64_t cs = size==1? (int8_t)cur : size==2? (int16_t)cur : (int32_t)cur;
      int64_t ps = size==1? (int8_t)prev : size==2? (int16_t)prev : (int32_t)prev;
      bool pass = false;
      switch(compare){
        case SE_SEARCH_EQUAL: pass = cur==value; break;
        case SE_SEARCH_NOT_EQUAL: pass = cur!=value; break;
        case SE_SEARCH_GREATER: pass = is_signed? cs>(int64_t)value : cur>value; break;
        case SE_SEARCH_LESS: pass = is_signed? cs<(int64_t)value : cur<value; break;
        case SE_SEARCH_CHANGED: pass = cur!=prev; break;
        case SE_SEARCH_UNCHANGED: pass = cur==prev; break;
        case SE_SEARCH_INCREASED: pass = is_signed? cs>ps : cur>prev; break;
        case SE_SEARCH_DECREASED: pass = is_signed? cs<ps : cur<prev; break;
        case SE_SEARCH_INCREASED_BY: pass = cur!=prev&&((cur-prev)&mask)==value; break;
        case SE_SEARCH_DECREASED_BY: pass = cur!=prev&&((prev-cur)&mask)==value; break;
        default: break;
      }
      expected+=pass;
    }
    uint32_t got = se_search_filter(&search,b,compare,value);
    CHECK(got==expected,"random search %d: size %d compare %d value %u: %u vs %u",round,size,compare,value,got,expected);
  }
  se_search_reset(&search);
  CHECK(!se_search_active(&search),"reset");
  CHECK(se_search_filter(&search,memory,SE_SEARCH_EQUAL,0)==0,"filter without a search");
  CHECK(!se_search_start(&search,regions,0,memory,1,1,false),"no regions");
  CHECK(!se_search_start(&search,regions,2,memory,3,1,false),"bad value size");
}

int main(void){
  test_arv3();
  test_codes();
  test_search();
  if(failures){
    printf("%d cheat finder checks failed\n",failures);
    return 1;
  }
  printf("All cheat finder checks passed\n");
  return 0;
}
