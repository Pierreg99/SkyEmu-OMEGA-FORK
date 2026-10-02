/*****************************************************************************
 *
 *   SkyEmu screen layouts
 *
 *   Where the two screens of the Nintendo DS go. A layout places each screen
 *   in "layout pixels", where a DS screen is 256x192 at full size, and gives
 *   the size of the box around them; the GUI scales that box to the window.
 *   Has no dependencies so it can be unit tested on its own (see
 *   tools/se_screen_layout_test.c).
 *
**/
#ifndef SE_SCREEN_LAYOUT_H
#define SE_SCREEN_LAYOUT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SE_NDS_LAYOUT_AUTO 0                  // Vertical, or Hybrid Large Top in wide windows
#define SE_NDS_LAYOUT_VERTICAL 1              // Top screen above the bottom screen
#define SE_NDS_LAYOUT_HORIZONTAL 2            // Side by side
#define SE_NDS_LAYOUT_HYBRID_LARGE_TOP 3      // Large top screen, both screens small beside it
#define SE_NDS_LAYOUT_HYBRID_LARGE_BOTTOM 4   // Large bottom screen, both screens small beside it
#define SE_NDS_LAYOUT_VERTICAL_LARGE_TOP 5
#define SE_NDS_LAYOUT_VERTICAL_LARGE_BOTTOM 6
#define SE_NDS_LAYOUT_HORIZONTAL_LARGE_TOP 7
#define SE_NDS_LAYOUT_HORIZONTAL_LARGE_BOTTOM 8
#define SE_NDS_LAYOUT_TOP_ONLY 9
#define SE_NDS_LAYOUT_BOTTOM_ONLY 10
#define SE_NDS_NUM_LAYOUTS 11

#define SE_NDS_SCREEN_TOP 0
#define SE_NDS_SCREEN_BOTTOM 1                // The touch screen

typedef struct{
  float x, y, w, h;  // Top left corner and size in layout pixels
  int screen;        // SE_NDS_SCREEN_TOP or SE_NDS_SCREEN_BOTTOM
}se_screen_place_t;

// Places the screens of a layout (not SE_NDS_LAYOUT_AUTO, resolve it first). gap is the space
// between the screens in layout pixels, small_size the size of the small screen of the large and
// hybrid layouts (0.25 to 1, 0.5 is the classic size) and swap exchanges the two screens, so the
// bottom screen goes where the top screen would. Writes up to 3 places (the hybrid layouts show
// one screen twice) and returns how many, 0 for an unknown layout.
int se_nds_layout_place(int layout, float gap, float small_size, bool swap, se_screen_place_t out[3],
                        float* box_w, float* box_h);

#ifdef __cplusplus
}
#endif

#endif
