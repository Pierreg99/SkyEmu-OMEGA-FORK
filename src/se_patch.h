/*****************************************************************************
 *
 *   SkyEmu ROM patches
 *
 *   Applies IPS, UPS and BPS patches to a ROM in memory ("soft patching"),
 *   so ROM hacks, translations and fixes can be played without changing the
 *   ROM file. Has no dependencies so it can be unit tested on its own (see
 *   tools/se_patch_test.c).
 *
**/
#ifndef SE_PATCH_H
#define SE_PATCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum{
  SE_PATCH_UNKNOWN = 0,
  SE_PATCH_IPS,
  SE_PATCH_UPS,
  SE_PATCH_BPS,
}se_patch_format_t;

// File extensions SkyEmu looks for next to a ROM, in the order they are applied
#define SE_PATCH_NUM_EXTENSIONS 3
extern const char* se_patch_extensions[SE_PATCH_NUM_EXTENSIONS]; // ".ips", ".ups", ".bps"

se_patch_format_t se_patch_detect(const uint8_t* patch, size_t patch_size);
const char* se_patch_format_name(se_patch_format_t format);

// Applies a patch to a ROM. On success *out (allocated with malloc, owned by the caller) and
// *out_size hold the patched ROM. On failure returns false, *out is NULL and *error explains why.
// UPS and BPS patches are checked against the ROM they were made for and the result they
// produce, IPS patches carry no checksums.
bool se_patch_apply(const uint8_t* rom, size_t rom_size, const uint8_t* patch, size_t patch_size,
                    uint8_t** out, size_t* out_size, const char** error);

// CRC-32 (the zlib / PNG polynomial) used by UPS and BPS
uint32_t se_patch_crc32(const uint8_t* data, size_t size);

#ifdef __cplusplus
}
#endif

#endif
