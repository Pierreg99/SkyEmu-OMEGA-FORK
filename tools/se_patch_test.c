// Unit test for the ROM patch engine (src/se_patch.c).
// Build and run from the repository root:
//   cc -O2 -Isrc tools/se_patch_test.c src/se_patch.c -o se_patch_test && ./se_patch_test
// The test writes IPS, UPS and BPS patches with small encoders following the format
// specifications, applies them, and checks damaged and mismatched patches are refused.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "se_patch.h"

static int failures = 0;
#define CHECK(cond, ...) do{ if(!(cond)){ printf("FAIL %s:%d: ",__FILE__,__LINE__); printf(__VA_ARGS__); printf("\n"); failures++; } }while(0)

typedef struct{uint8_t* data; size_t size, cap;}buf_t;
static void put(buf_t* b, const void* d, size_t n){
  if(b->size+n>b->cap){b->cap = (b->size+n)*2+64; b->data = realloc(b->data,b->cap);}
  memcpy(b->data+b->size,d,n);
  b->size+=n;
}
static void put8(buf_t* b, uint8_t v){put(b,&v,1);}
static void put_le32(buf_t* b, uint32_t v){uint8_t d[4]={v,v>>8,v>>16,v>>24}; put(b,d,4);}
// UPS / BPS number encoding
static void put_number(buf_t* b, uint64_t v){
  for(;;){
    uint8_t x = v&0x7f;
    v >>= 7;
    if(v==0){put8(b,x|0x80); break;}
    put8(b,x);
    v--;
  }
}
static uint32_t rng_state = 12345;
static uint8_t rnd(void){rng_state = rng_state*1103515245u+12345u; return rng_state>>16;}

// IPS: one record per changed run, RLE for long runs of one byte, truncation when shrinking
static buf_t make_ips(const uint8_t* src, size_t src_size, const uint8_t* dst, size_t dst_size){
  buf_t b = {0};
  put(&b,"PATCH",5);
  size_t i = 0;
  while(i<dst_size){
    if(i<src_size&&src[i]==dst[i]){i++; continue;}
    size_t start = i, run = 1;
    while(start+run<dst_size&&dst[start+run]==dst[start]&&run<0xffff)run++;
    if(start==0x454F46){i++; continue;} // not representable, skipped by this simple encoder
    if(run>=8){
      uint8_t rec[8]={start>>16,start>>8,start,0,0,run>>8,run,dst[start]};
      put(&b,rec,8);
      i = start+run;
      continue;
    }
    size_t len = 0;
    while(start+len<dst_size&&len<0xffff&&!(start+len<src_size&&src[start+len]==dst[start+len]))len++;
    uint8_t rec[5]={start>>16,start>>8,start,len>>8,len};
    put(&b,rec,5);
    put(&b,dst+start,len);
    i = start+len;
  }
  put(&b,"EOF",3);
  if(dst_size<src_size){uint8_t t[3]={dst_size>>16,dst_size>>8,dst_size}; put(&b,t,3);}
  return b;
}

static buf_t make_ups(const uint8_t* src, size_t src_size, const uint8_t* dst, size_t dst_size){
  buf_t b = {0};
  put(&b,"UPS1",4);
  put_number(&b,src_size);
  put_number(&b,dst_size);
  size_t max = src_size>dst_size? src_size : dst_size, last = 0, i = 0;
  while(i<max){
    uint8_t s = i<src_size? src[i] : 0, d = i<dst_size? dst[i] : 0;
    if(s==d){i++; continue;}
    put_number(&b,i-last);
    for(;;){
      s = i<src_size? src[i] : 0; d = i<dst_size? dst[i] : 0;
      put8(&b,s^d);
      i++;
      if(s==d)break;
      if(i>=max){put8(&b,0); i++; break;}
    }
    last = i;
  }
  put_le32(&b,se_patch_crc32(src,src_size));
  put_le32(&b,se_patch_crc32(dst,dst_size));
  put_le32(&b,se_patch_crc32(b.data,b.size));
  return b;
}

// BPS using all four commands: SourceRead where the bytes match, SourceCopy / TargetCopy when
// the next bytes repeat earlier ROM or output data, TargetRead otherwise
static buf_t make_bps(const uint8_t* src, size_t src_size, const uint8_t* dst, size_t dst_size){
  buf_t b = {0};
  put(&b,"BPS1",4);
  put_number(&b,src_size);
  put_number(&b,dst_size);
  const char* meta = "test";
  put_number(&b,strlen(meta));
  put(&b,meta,strlen(meta));
  size_t out = 0;
  int64_t src_rel = 0, dst_rel = 0;
  while(out<dst_size){
    size_t len = 0;
    while(out+len<dst_size&&out+len<src_size&&src[out+len]==dst[out+len])len++;
    if(len>=4){put_number(&b,((uint64_t)(len-1)<<2)|0); out+=len; continue;}
    // Look for a copy of 16 bytes or more at a few candidate offsets
    size_t best_len = 0, best_off = 0; int best_kind = 0;
    for(size_t cand=0;cand<src_size;cand+=97){
      size_t l=0; while(out+l<dst_size&&cand+l<src_size&&src[cand+l]==dst[out+l])l++;
      if(l>best_len){best_len=l; best_off=cand; best_kind=2;}
    }
    for(size_t cand=0;cand+1<=out;cand+=13){
      size_t l=0; while(out+l<dst_size&&cand+l<out+l&&dst[cand+l]==dst[out+l])l++;
      if(l>best_len){best_len=l; best_off=cand; best_kind=3;}
    }
    if(best_len>=16){
      put_number(&b,((uint64_t)(best_len-1)<<2)|best_kind);
      int64_t* rel = best_kind==2? &src_rel : &dst_rel;
      int64_t delta = (int64_t)best_off-*rel;
      put_number(&b,delta<0? ((uint64_t)(-delta)<<1)|1 : (uint64_t)delta<<1);
      *rel = (int64_t)(best_off+best_len);
      out+=best_len;
      continue;
    }
    size_t n = 1;
    while(out+n<dst_size&&n<64&&!(out+n<src_size&&src[out+n]==dst[out+n]))n++;
    put_number(&b,((uint64_t)(n-1)<<2)|1);
    put(&b,dst+out,n);
    out+=n;
  }
  put_le32(&b,se_patch_crc32(src,src_size));
  put_le32(&b,se_patch_crc32(dst,dst_size));
  put_le32(&b,se_patch_crc32(b.data,b.size));
  return b;
}

static void make_case(int kind, uint8_t** src, size_t* src_size, uint8_t** dst, size_t* dst_size){
  *src_size = 4096+(rnd()<<4);
  *src = malloc(*src_size);
  for(size_t i=0;i<*src_size;++i)(*src)[i] = rnd();
  *dst_size = kind==1? *src_size+3000 : kind==2? *src_size/2 : *src_size;
  *dst = calloc(*dst_size,1);
  memcpy(*dst,*src,*dst_size<*src_size? *dst_size : *src_size);
  for(int e=0;e<40;++e){
    size_t at = (((size_t)rnd()<<8)|rnd())%*dst_size;
    for(int k=0;k<10&&at+k<*dst_size;++k)(*dst)[at+k] = rnd();
  }
  if(kind==1){
    memset(*dst+*src_size,0xff,1000);                           // RLE run in the grown part
    memcpy(*dst+*src_size+1000,*src+100,1000);                  // copy of ROM data
  }
  if(kind==3&&*dst_size>600)memcpy(*dst+500,*dst+50,100);       // repeated output data
}

static void test_roundtrips(void){
  const char* names[] = {"same size","grown","shrunk","repeats"};
  for(int kind=0;kind<4;++kind){
    uint8_t *src, *dst; size_t src_size, dst_size;
    make_case(kind,&src,&src_size,&dst,&dst_size);
    buf_t patches[3] = {make_ips(src,src_size,dst,dst_size),make_ups(src,src_size,dst,dst_size),make_bps(src,src_size,dst,dst_size)};
    se_patch_format_t formats[3] = {SE_PATCH_IPS,SE_PATCH_UPS,SE_PATCH_BPS};
    for(int f=0;f<3;++f){
      CHECK(se_patch_detect(patches[f].data,patches[f].size)==formats[f],"detect %s",se_patch_format_name(formats[f]));
      uint8_t* out; size_t out_size; const char* error;
      bool ok = se_patch_apply(src,src_size,patches[f].data,patches[f].size,&out,&out_size,&error);
      CHECK(ok,"%s %s: %s",se_patch_format_name(formats[f]),names[kind],error? error : "");
      if(ok){
        CHECK(out_size==dst_size&&memcmp(out,dst,dst_size)==0,"%s %s: wrong result",se_patch_format_name(formats[f]),names[kind]);
        free(out);
      }
    }
    // UPS and BPS refuse a different ROM, IPS cannot tell
    src[1]^=0x40;
    for(int f=1;f<3;++f){
      uint8_t* out; size_t out_size; const char* error;
      CHECK(!se_patch_apply(src,src_size,patches[f].data,patches[f].size,&out,&out_size,&error)&&out==NULL&&strstr(error,"different ROM"),
            "%s %s: accepted a different ROM",se_patch_format_name(formats[f]),names[kind]);
    }
    src[1]^=0x40;
    // A flipped byte in the patch is caught by the patch checksum
    for(int f=1;f<3;++f){
      buf_t* p = &patches[f];
      p->data[p->size/2]^=1;
      uint8_t* out; size_t out_size; const char* error;
      CHECK(!se_patch_apply(src,src_size,p->data,p->size,&out,&out_size,&error)&&strstr(error,"damaged"),
            "%s %s: accepted a damaged patch",se_patch_format_name(formats[f]),names[kind]);
      p->data[p->size/2]^=1;
    }
    // Truncated patches are refused (or caught by a checksum), never read out of bounds
    for(int f=0;f<3;++f){
      for(size_t cut=1;cut<patches[f].size;cut+=patches[f].size/7+1){
        uint8_t* out; size_t out_size; const char* error;
        bool ok = se_patch_apply(src,src_size,patches[f].data,patches[f].size-cut,&out,&out_size,&error);
        if(ok)free(out);
        CHECK(!ok||f==0,"%s %s: accepted a truncated patch",se_patch_format_name(formats[f]),names[kind]);
      }
      free(patches[f].data);
    }
    free(src); free(dst);
  }
}

static void test_ips_details(void){
  uint8_t rom[16]; memset(rom,0x11,sizeof(rom));
  // RLE record writing 4 bytes of 0xAB at 0x02, then a normal record past the end of the ROM
  const uint8_t ips[] = {'P','A','T','C','H', 0,0,2, 0,0, 0,4, 0xAB, 0,0,0x12, 0,2, 0xCD,0xEF, 'E','O','F'};
  uint8_t* out; size_t out_size; const char* error;
  CHECK(se_patch_apply(rom,sizeof(rom),ips,sizeof(ips),&out,&out_size,&error),"IPS details: %s",error);
  if(out){
    CHECK(out_size==20,"IPS grows the ROM to the last record (%zu)",out_size);
    CHECK(out[1]==0x11&&out[2]==0xAB&&out[5]==0xAB&&out[6]==0x11,"IPS RLE record");
    CHECK(out[16]==0&&out[18]==0xCD&&out[19]==0xEF,"IPS record past the end, gap filled with zeros");
    free(out);
  }
  const uint8_t no_eof[] = {'P','A','T','C','H', 0,0,2, 0,1, 0x55};
  CHECK(!se_patch_apply(rom,sizeof(rom),no_eof,sizeof(no_eof),&out,&out_size,&error),"IPS without EOF is refused");
  const uint8_t junk[] = {'N','O','P','E',1,2,3,4,5,6,7,8,9,10,11,12};
  CHECK(!se_patch_apply(rom,sizeof(rom),junk,sizeof(junk),&out,&out_size,&error)&&strstr(error,"Unknown"),"unknown format");
}

static void test_bps_result_check(void){
  uint8_t src[64], dst[64];
  for(int i=0;i<64;++i){src[i] = i; dst[i] = 63-i;}
  buf_t p = make_bps(src,64,dst,64);
  // Wrong target CRC with a valid patch CRC: the result check must catch it
  size_t target_crc_at = p.size-8;
  p.data[target_crc_at]^=1;
  uint32_t crc = se_patch_crc32(p.data,p.size-4);
  p.data[p.size-4]=crc; p.data[p.size-3]=crc>>8; p.data[p.size-2]=crc>>16; p.data[p.size-1]=crc>>24;
  uint8_t* out; size_t out_size; const char* error;
  CHECK(!se_patch_apply(src,64,p.data,p.size,&out,&out_size,&error)&&strstr(error,"does not match"),"BPS result checksum");
  free(p.data);
}

static void test_fuzz(void){
  // Random damage to valid patches must never crash or read out of bounds
  uint8_t *src, *dst; size_t src_size, dst_size;
  make_case(1,&src,&src_size,&dst,&dst_size);
  buf_t patches[3] = {make_ips(src,src_size,dst,dst_size),make_ups(src,src_size,dst,dst_size),make_bps(src,src_size,dst,dst_size)};
  for(int f=0;f<3;++f){
    for(int round=0;round<300;++round){
      uint8_t* copy = malloc(patches[f].size);
      memcpy(copy,patches[f].data,patches[f].size);
      for(int k=0;k<4;++k)copy[(((size_t)rnd()<<8)|rnd())%patches[f].size] = rnd();
      // Keep the patch CRC valid half of the time so the parser itself is exercised
      if(f>0&&(round&1)){
        uint32_t crc = se_patch_crc32(copy,patches[f].size-4);
        size_t e = patches[f].size;
        copy[e-4]=crc; copy[e-3]=crc>>8; copy[e-2]=crc>>16; copy[e-1]=crc>>24;
      }
      uint8_t* out; size_t out_size; const char* error;
      if(se_patch_apply(src,src_size,copy,patches[f].size,&out,&out_size,&error))free(out);
      free(copy);
    }
    free(patches[f].data);
  }
  free(src); free(dst);
}

int main(void){
  CHECK(se_patch_crc32((const uint8_t*)"123456789",9)==0xCBF43926u,"CRC-32 check value");
  test_roundtrips();
  test_ips_details();
  test_bps_result_check();
  test_fuzz();
  if(failures){printf("%d check(s) failed\n",failures); return 1;}
  printf("All patch checks passed\n");
  return 0;
}
