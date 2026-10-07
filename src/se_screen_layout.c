/*****************************************************************************
 *
 *   SkyEmu screen layouts, see se_screen_layout.h
 *
**/
#include "se_screen_layout.h"

#define SE_DS_SCREEN_W 256.0f
#define SE_DS_SCREEN_H 192.0f

static se_screen_place_t se_screen_place(float x, float y, float w, float h, int screen){
  se_screen_place_t p = {x,y,w,h,screen};
  return p;
}

int se_nds_layout_place(int layout, float gap, float small_size, bool swap, se_screen_place_t out[3],
                        float* box_w, float* box_h){
  const float W = SE_DS_SCREEN_W, H = SE_DS_SCREEN_H;
  if(!(gap>0))gap = 0;
  if(!(small_size>0))small_size = 0.5f;
  if(small_size<0.25f)small_size = 0.25f;
  if(small_size>1.0f)small_size = 1.0f;
  const float sw = W*small_size, sh = H*small_size;
  // first is the screen in the place of the top screen, second the one in the place of the bottom screen
  const int first = swap? SE_NDS_SCREEN_BOTTOM : SE_NDS_SCREEN_TOP;
  const int second = swap? SE_NDS_SCREEN_TOP : SE_NDS_SCREEN_BOTTOM;
  float bw = 0, bh = 0;
  int n = 0;
  switch(layout){
    case SE_NDS_LAYOUT_VERTICAL:
      out[n++] = se_screen_place(0,0,W,H,first);
      out[n++] = se_screen_place(0,H+gap,W,H,second);
      bw = W; bh = H*2+gap;
      break;
    case SE_NDS_LAYOUT_HORIZONTAL:
      out[n++] = se_screen_place(0,0,W,H,first);
      out[n++] = se_screen_place(W+gap,0,W,H,second);
      bw = W*2+gap; bh = H;
      break;
    case SE_NDS_LAYOUT_HYBRID_LARGE_TOP:
    case SE_NDS_LAYOUT_HYBRID_LARGE_BOTTOM:{
      // The large screen on the left, both screens small in a column on the right
      float column_h = sh*2+gap;
      bh = column_h>H? column_h : H;
      bw = W+gap+sw;
      float y = (bh-column_h)*0.5f;
      out[n++] = se_screen_place(W+gap,y,sw,sh,first);
      out[n++] = se_screen_place(W+gap,y+sh+gap,sw,sh,second);
      int large = layout==SE_NDS_LAYOUT_HYBRID_LARGE_TOP? first : second;
      out[n++] = se_screen_place(0,(bh-H)*0.5f,W,H,large);
    }break;
    case SE_NDS_LAYOUT_VERTICAL_LARGE_TOP:
      out[n++] = se_screen_place(0,0,W,H,first);
      out[n++] = se_screen_place((W-sw)*0.5f,H+gap,sw,sh,second);
      bw = W; bh = H+gap+sh;
      break;
    case SE_NDS_LAYOUT_VERTICAL_LARGE_BOTTOM:
      out[n++] = se_screen_place((W-sw)*0.5f,0,sw,sh,first);
      out[n++] = se_screen_place(0,sh+gap,W,H,second);
      bw = W; bh = sh+gap+H;
      break;
    case SE_NDS_LAYOUT_HORIZONTAL_LARGE_TOP:
      out[n++] = se_screen_place(0,0,W,H,first);
      out[n++] = se_screen_place(W+gap,(H-sh)*0.5f,sw,sh,second);
      bw = W+gap+sw; bh = H;
      break;
    case SE_NDS_LAYOUT_HORIZONTAL_LARGE_BOTTOM:
      out[n++] = se_screen_place(0,(H-sh)*0.5f,sw,sh,first);
      out[n++] = se_screen_place(sw+gap,0,W,H,second);
      bw = sw+gap+W; bh = H;
      break;
    // Swap shows the other screen, so the swap hotkey switches between the two
    case SE_NDS_LAYOUT_TOP_ONLY:
      out[n++] = se_screen_place(0,0,W,H,first);
      bw = W; bh = H;
      break;
    case SE_NDS_LAYOUT_BOTTOM_ONLY:
      out[n++] = se_screen_place(0,0,W,H,second);
      bw = W; bh = H;
      break;
    default: break;
  }
  if(box_w)*box_w = bw;
  if(box_h)*box_h = bh;
  return n;
}
