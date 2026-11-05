#ifndef SE_CHEAT_FINDER_H
#define SE_CHEAT_FINDER_H
// RAM search ("cheat finder"): snapshot the console's work RAM, then narrow the candidate addresses down with
// comparisons against a value or against the previous snapshot. Memory is accessed through callbacks so the
// search works the same for every core and can be tested without one.
#include <stdbool.h>
#include <stdint.h>

#define SE_CF_MAX_REGIONS 4
#define SE_CF_MAX_FROZEN 16

typedef uint8_t (*se_cf_read_fn)(uint64_t address);
typedef void (*se_cf_write_fn)(uint64_t address, uint8_t data);

typedef enum{
  SE_CF_EQUAL,          // == value
  SE_CF_NOT_EQUAL,      // != value
  SE_CF_GREATER,        // >  value
  SE_CF_LESS,           // <  value
  SE_CF_CHANGED,        // != previous snapshot
  SE_CF_UNCHANGED,      // == previous snapshot
  SE_CF_INCREASED,      // >  previous snapshot
  SE_CF_DECREASED,      // <  previous snapshot
  SE_CF_OP_COUNT
}se_cf_op_t;

typedef struct{
  uint32_t base;
  uint32_t size;
  const char* name;
}se_cf_region_desc_t;

typedef struct{
  se_cf_region_desc_t desc;
  uint8_t* cur;
  uint8_t* prev;
  uint8_t* candidates; // one bit per byte offset; only offsets aligned to value_size are ever set
}se_cf_region_t;

typedef struct{
  uint32_t address;
  uint32_t value;
  uint8_t size;
}se_cf_frozen_t;

typedef struct{
  se_cf_region_t regions[SE_CF_MAX_REGIONS];
  int num_regions;
  int value_size; // 1, 2 or 4 bytes, little endian
  uint32_t num_candidates;
  int searches; // filters applied since se_cf_begin
  se_cf_frozen_t frozen[SE_CF_MAX_FROZEN];
  int num_frozen;
}se_cheat_finder_t;

typedef struct{
  uint32_t address;
  uint32_t value;
  uint32_t previous;
}se_cf_result_t;

// Frees every snapshot; frozen values are kept.
void se_cf_reset(se_cheat_finder_t* cf);
// Starts a new search over the given regions: every aligned address is a candidate.
bool se_cf_begin(se_cheat_finder_t* cf, const se_cf_region_desc_t* regions, int num_regions, int value_size, se_cf_read_fn read);
// Takes a new snapshot and keeps the candidates that satisfy op. Returns the number of candidates left.
uint32_t se_cf_filter(se_cheat_finder_t* cf, se_cf_op_t op, uint32_t value, se_cf_read_fn read);
// Writes up to max_results candidates (lowest addresses first) and returns how many were written.
int se_cf_results(const se_cheat_finder_t* cf, se_cf_result_t* out, int max_results);

uint32_t se_cf_read_value(se_cf_read_fn read, uint32_t address, int size);
void se_cf_write_value(se_cf_write_fn write, uint32_t address, uint32_t value, int size);
// Frozen addresses are rewritten every frame. Freezing an address again updates its value.
bool se_cf_freeze(se_cheat_finder_t* cf, uint32_t address, uint32_t value, int size);
void se_cf_unfreeze(se_cheat_finder_t* cf, int index);
void se_cf_apply_frozen(const se_cheat_finder_t* cf, se_cf_write_fn write);
#endif
