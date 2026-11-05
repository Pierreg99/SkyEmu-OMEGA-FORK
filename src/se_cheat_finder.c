#include "se_cheat_finder.h"
#include <stdlib.h>
#include <string.h>

static uint32_t se_cf_mask(int size){ return size>=4? 0xffffffffu : (1u<<(size*8))-1u; }

static uint32_t se_cf_snapshot_value(const uint8_t* data, uint32_t offset, int size){
  uint32_t v = 0;
  for(int i=0;i<size;++i)v|=(uint32_t)data[offset+i]<<(i*8);
  return v;
}

void se_cf_reset(se_cheat_finder_t* cf){
  for(int r=0;r<cf->num_regions;++r){
    free(cf->regions[r].cur);
    free(cf->regions[r].prev);
    free(cf->regions[r].candidates);
  }
  memset(cf->regions,0,sizeof(cf->regions));
  cf->num_regions = 0;
  cf->num_candidates = 0;
  cf->searches = 0;
}

static void se_cf_read_region(se_cf_region_t* region, se_cf_read_fn read){
  for(uint32_t i=0;i<region->desc.size;++i)region->cur[i]=read((uint64_t)region->desc.base+i);
}

bool se_cf_begin(se_cheat_finder_t* cf, const se_cf_region_desc_t* regions, int num_regions, int value_size, se_cf_read_fn read){
  se_cf_reset(cf);
  if(value_size!=1&&value_size!=2&&value_size!=4)value_size=1;
  if(num_regions>SE_CF_MAX_REGIONS)num_regions=SE_CF_MAX_REGIONS;
  cf->value_size = value_size;
  for(int r=0;r<num_regions;++r){
    se_cf_region_t* region = &cf->regions[r];
    region->desc = regions[r];
    uint32_t size = region->desc.size;
    region->cur = (uint8_t*)malloc(size);
    region->prev = (uint8_t*)malloc(size);
    region->candidates = (uint8_t*)calloc((size+7)/8,1);
    cf->num_regions = r+1;
    if(!region->cur||!region->prev||!region->candidates){ se_cf_reset(cf); return false; }
    se_cf_read_region(region,read);
    memcpy(region->prev,region->cur,size);
    for(uint32_t i=0;i+value_size<=size;i+=value_size){
      region->candidates[i>>3]|=1u<<(i&7);
      cf->num_candidates++;
    }
  }
  return true;
}

static bool se_cf_test(se_cf_op_t op, uint32_t cur, uint32_t prev, uint32_t value){
  switch(op){
    case SE_CF_EQUAL:     return cur==value;
    case SE_CF_NOT_EQUAL: return cur!=value;
    case SE_CF_GREATER:   return cur>value;
    case SE_CF_LESS:      return cur<value;
    case SE_CF_CHANGED:   return cur!=prev;
    case SE_CF_UNCHANGED: return cur==prev;
    case SE_CF_INCREASED: return cur>prev;
    case SE_CF_DECREASED: return cur<prev;
    default:              return false;
  }
}

uint32_t se_cf_filter(se_cheat_finder_t* cf, se_cf_op_t op, uint32_t value, se_cf_read_fn read){
  value &= se_cf_mask(cf->value_size);
  cf->num_candidates = 0;
  for(int r=0;r<cf->num_regions;++r){
    se_cf_region_t* region = &cf->regions[r];
    uint8_t* swap = region->prev; region->prev = region->cur; region->cur = swap;
    se_cf_read_region(region,read);
    for(uint32_t i=0;i+cf->value_size<=region->desc.size;i+=cf->value_size){
      if(!(region->candidates[i>>3]&(1u<<(i&7))))continue;
      uint32_t cur = se_cf_snapshot_value(region->cur,i,cf->value_size);
      uint32_t prev = se_cf_snapshot_value(region->prev,i,cf->value_size);
      if(se_cf_test(op,cur,prev,value))cf->num_candidates++;
      else region->candidates[i>>3]&=~(1u<<(i&7));
    }
  }
  cf->searches++;
  return cf->num_candidates;
}

int se_cf_results(const se_cheat_finder_t* cf, se_cf_result_t* out, int max_results){
  int n = 0;
  for(int r=0;r<cf->num_regions&&n<max_results;++r){
    const se_cf_region_t* region = &cf->regions[r];
    for(uint32_t i=0;i+cf->value_size<=region->desc.size&&n<max_results;i+=cf->value_size){
      if(!(region->candidates[i>>3]&(1u<<(i&7))))continue;
      out[n].address = region->desc.base+i;
      out[n].value = se_cf_snapshot_value(region->cur,i,cf->value_size);
      out[n].previous = se_cf_snapshot_value(region->prev,i,cf->value_size);
      n++;
    }
  }
  return n;
}

uint32_t se_cf_read_value(se_cf_read_fn read, uint32_t address, int size){
  uint32_t v = 0;
  for(int i=0;i<size;++i)v|=(uint32_t)read((uint64_t)address+i)<<(i*8);
  return v;
}

void se_cf_write_value(se_cf_write_fn write, uint32_t address, uint32_t value, int size){
  for(int i=0;i<size;++i)write((uint64_t)address+i,(uint8_t)(value>>(i*8)));
}

bool se_cf_freeze(se_cheat_finder_t* cf, uint32_t address, uint32_t value, int size){
  for(int i=0;i<cf->num_frozen;++i){
    if(cf->frozen[i].address==address){
      cf->frozen[i].value = value&se_cf_mask(size);
      cf->frozen[i].size = (uint8_t)size;
      return true;
    }
  }
  if(cf->num_frozen>=SE_CF_MAX_FROZEN)return false;
  cf->frozen[cf->num_frozen++] = (se_cf_frozen_t){address,value&se_cf_mask(size),(uint8_t)size};
  return true;
}

void se_cf_unfreeze(se_cheat_finder_t* cf, int index){
  if(index<0||index>=cf->num_frozen)return;
  memmove(&cf->frozen[index],&cf->frozen[index+1],sizeof(cf->frozen[0])*(cf->num_frozen-index-1));
  cf->num_frozen--;
}

void se_cf_apply_frozen(const se_cheat_finder_t* cf, se_cf_write_fn write){
  for(int i=0;i<cf->num_frozen;++i)se_cf_write_value(write,cf->frozen[i].address,cf->frozen[i].value,cf->frozen[i].size);
}
