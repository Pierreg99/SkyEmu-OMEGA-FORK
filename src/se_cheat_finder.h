/*****************************************************************************
 *
 *   SkyEmu cheat finder
 *
 *   Finds where a game keeps a value (lives, money, health...) by searching
 *   its RAM again and again while the value changes, then turns an address
 *   into an Action Replay / GameShark code for the console. Has no
 *   dependencies so it can be unit tested on its own (see
 *   tools/se_cheat_finder_test.c).
 *
**/
#ifndef SE_CHEAT_FINDER_H
#define SE_CHEAT_FINDER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum{
  SE_SEARCH_EQUAL = 0,     // Equal to the value
  SE_SEARCH_NOT_EQUAL,     // Not equal to the value
  SE_SEARCH_GREATER,       // Greater than the value
  SE_SEARCH_LESS,          // Less than the value
  SE_SEARCH_CHANGED,       // Changed since the last search
  SE_SEARCH_UNCHANGED,     // Did not change since the last search
  SE_SEARCH_INCREASED,     // Increased since the last search
  SE_SEARCH_DECREASED,     // Decreased since the last search
  SE_SEARCH_INCREASED_BY,  // Increased by exactly the value
  SE_SEARCH_DECREASED_BY,  // Decreased by exactly the value
  SE_SEARCH_NUM_COMPARES,
}se_search_compare_t;

// True when the comparison uses the value typed by the user
bool se_search_compare_uses_value(se_search_compare_t compare);

#define SE_SEARCH_MAX_REGIONS 4

// A block of console memory, stored back to back with the others in the memory images
typedef struct{
  uint32_t address; // Console address of the first byte
  uint32_t size;
}se_search_region_t;

typedef struct{
  se_search_region_t regions[SE_SEARCH_MAX_REGIONS];
  int num_regions;
  uint32_t memory_size;  // Sum of the region sizes
  int value_size;        // 1, 2 or 4 bytes, little endian
  int alignment;         // Values start at multiples of this many bytes
  bool is_signed;        // Greater / less / increased / decreased compare signed values
  uint8_t* previous;     // Memory at the last search
  uint32_t* candidates;  // One bit per byte offset that still matches
  uint32_t num_candidates;
  int searches;          // Number of filters applied since the search started
}se_cheat_search_t;

// Starts a new search: every aligned value of the memory is a candidate. memory holds
// memory_size bytes laid out as the regions. Returns false when out of memory.
bool se_search_start(se_cheat_search_t* search, const se_search_region_t* regions, int num_regions,
                     const uint8_t* memory, int value_size, int alignment, bool is_signed);
// Keeps the candidates whose value in memory passes the comparison and remembers memory for
// the next search. Returns the number of candidates left.
uint32_t se_search_filter(se_cheat_search_t* search, const uint8_t* memory, se_search_compare_t compare, uint32_t value);
// Frees the search, it can be started again
void se_search_reset(se_cheat_search_t* search);
bool se_search_active(const se_cheat_search_t* search);
// Writes up to max candidate offsets, from first_result on, in address order. Returns how many.
uint32_t se_search_results(const se_cheat_search_t* search, uint32_t first_result, uint32_t* offsets, uint32_t max);
// Console address of an offset in the memory images (0 if out of range)
uint32_t se_search_address(const se_cheat_search_t* search, uint32_t offset);
// Little endian value at an offset of a memory image
uint32_t se_search_read(const uint8_t* memory, uint32_t offset, int value_size);

typedef enum{
  SE_CHEAT_SYSTEM_GB = 0, // Game Boy / Game Boy Color: GameShark codes
  SE_CHEAT_SYSTEM_GBA,    // Game Boy Advance: Action Replay v3 codes (encrypted)
  SE_CHEAT_SYSTEM_NDS,    // Nintendo DS: Action Replay codes
}se_cheat_system_t;

// Makes a code that keeps value_size bytes at address set to value, in the format SkyEmu's
// cheat engine reads. Writes up to max_words 32 bit words to words and returns how many, or 0
// when the address can not be written by a code of that system.
int se_cheat_make_code(se_cheat_system_t system, uint32_t address, uint32_t value, int value_size,
                       uint32_t* words, int max_words);
// Formats code words as text for the cheat editor, two words per line ("XXXXXXXX YYYYYYYY")
void se_cheat_format_code(const uint32_t* words, int num_words, char* out, size_t out_size);

// Game Boy Advance Action Replay v3 encryption, the reverse of the decryption of the cheat engine
uint64_t se_gba_encrypt_arv3(uint64_t code);
uint64_t se_gba_decrypt_arv3(uint64_t code);

#ifdef __cplusplus
}
#endif

#endif
