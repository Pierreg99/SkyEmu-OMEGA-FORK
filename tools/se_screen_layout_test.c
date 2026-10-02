/*****************************************************************************
 *
 *   Unit tests of the DS screen layouts (src/se_screen_layout.c)
 *
 *   cc -O2 -Wall -Wextra -Isrc tools/se_screen_layout_test.c src/se_screen_layout.c -lm -o se_screen_layout_test
 *
 *   With no gap and the classic small screen the layouts must place the screens exactly where the
 *   hand written drawing code of earlier versions did. Then the gap, the small screen size, swap
 *   and the single screen layouts are checked for overlaps and for staying inside their box.
 *
**/
#include "se_screen_layout.h"

#include <math.h>
#include <stdio.h>

static int failures = 0;
#define CHECK(cond, ...) do{ if(!(cond)){ failures++; printf("FAIL %s:%d: ",__FILE__,__LINE__); printf(__VA_ARGS__); printf("\n"); } }while(0)

#define T SE_NDS_SCREEN_TOP
#define B SE_NDS_SCREEN_BOTTOM

// A screen as the old drawing code placed it: its center relative to the center of the box and its
// size, both as a fraction of the box
typedef struct{float cx, cy, w, h; int screen;}old_place_t;
typedef struct{
  int layout;
  float box_w, box_h;   // In DS screens
  int count;
  old_place_t p[3];
}old_layout_t;

static const old_layout_t old_layouts[] = {
  {SE_NDS_LAYOUT_VERTICAL, 1, 2, 2, {{0,-0.25f,1,0.5f,T},{0,0.25f,1,0.5f,B}}},
  // The old code put the top screen on the right, unlike Horizontal Large Top and every other DS
  // emulator; it is on the left now and Swap Screens gives the old order
  {SE_NDS_LAYOUT_HORIZONTAL, 2, 1, 2, {{-0.25f,0,0.5f,1,T},{0.25f,0,0.5f,1,B}}},
  {SE_NDS_LAYOUT_HYBRID_LARGE_TOP, 1.5f, 1, 3, {{1/3.f,-0.25f,1/3.f,0.5f,T},{1/3.f,0.25f,1/3.f,0.5f,B},{-1/6.f,0,2/3.f,1,T}}},
  {SE_NDS_LAYOUT_HYBRID_LARGE_BOTTOM, 1.5f, 1, 3, {{1/3.f,-0.25f,1/3.f,0.5f,T},{1/3.f,0.25f,1/3.f,0.5f,B},{-1/6.f,0,2/3.f,1,B}}},
  {SE_NDS_LAYOUT_VERTICAL_LARGE_TOP, 1, 1.5f, 2, {{0,-1/6.f,1,2/3.f,T},{0,1/3.f,0.5f,1/3.f,B}}},
  {SE_NDS_LAYOUT_VERTICAL_LARGE_BOTTOM, 1, 1.5f, 2, {{0,-1/3.f,0.5f,1/3.f,T},{0,1/6.f,1,2/3.f,B}}},
  {SE_NDS_LAYOUT_HORIZONTAL_LARGE_TOP, 1.5f, 1, 2, {{-1/6.f,0,2/3.f,1,T},{1/3.f,0,1/3.f,0.5f,B}}},
  {SE_NDS_LAYOUT_HORIZONTAL_LARGE_BOTTOM, 1.5f, 1, 2, {{-1/3.f,0,1/3.f,0.5f,T},{1/6.f,0,2/3.f,1,B}}},
};

static bool near(float a, float b){return fabsf(a-b)<1e-4f;}

static void test_classic_layouts(void){
  for(size_t i=0;i<sizeof(old_layouts)/sizeof(old_layouts[0]);++i){
    const old_layout_t* o = &old_layouts[i];
    se_screen_place_t p[3];
    float bw=0, bh=0;
    int n = se_nds_layout_place(o->layout,0,0.5f,false,p,&bw,&bh);
    CHECK(n==o->count,"layout %d: %d screens, expected %d",o->layout,n,o->count);
    CHECK(near(bw,o->box_w*256)&&near(bh,o->box_h*192),"layout %d: box %gx%g",o->layout,bw,bh);
    if(n!=o->count)continue;
    for(int s=0;s<n;++s){
      float cx = (p[s].x+p[s].w*0.5f-bw*0.5f)/bw;
      float cy = (p[s].y+p[s].h*0.5f-bh*0.5f)/bh;
      CHECK(near(cx,o->p[s].cx)&&near(cy,o->p[s].cy),"layout %d screen %d: center %g,%g, expected %g,%g",
            o->layout,s,cx,cy,o->p[s].cx,o->p[s].cy);
      CHECK(near(p[s].w/bw,o->p[s].w)&&near(p[s].h/bh,o->p[s].h),"layout %d screen %d: size %g,%g",
            o->layout,s,p[s].w/bw,p[s].h/bh);
      CHECK(p[s].screen==o->p[s].screen,"layout %d screen %d: shows screen %d",o->layout,s,p[s].screen);
    }
  }
}

static bool overlap(const se_screen_place_t* a, const se_screen_place_t* b){
  return a->x<b->x+b->w-1e-3f&&b->x<a->x+a->w-1e-3f&&a->y<b->y+b->h-1e-3f&&b->y<a->y+a->h-1e-3f;
}

// The space between two places along the axis where they do not overlap
static float spacing(const se_screen_place_t* a, const se_screen_place_t* b){
  float dx = fmaxf(b->x-(a->x+a->w),a->x-(b->x+b->w));
  float dy = fmaxf(b->y-(a->y+a->h),a->y-(b->y+b->h));
  return fmaxf(dx,dy);
}

static void test_all_options(void){
  const float gaps[] = {0,1,16,96};
  const float sizes[] = {0.25f,0.5f,0.6f,1.0f};
  for(int layout=1;layout<SE_NDS_NUM_LAYOUTS;++layout)
  for(size_t g=0;g<sizeof(gaps)/sizeof(gaps[0]);++g)
  for(size_t s=0;s<sizeof(sizes)/sizeof(sizes[0]);++s)
  for(int swap=0;swap<2;++swap){
    se_screen_place_t p[3];
    float bw=0, bh=0;
    int n = se_nds_layout_place(layout,gaps[g],sizes[s],swap,p,&bw,&bh);
    int expected = layout==SE_NDS_LAYOUT_TOP_ONLY||layout==SE_NDS_LAYOUT_BOTTOM_ONLY? 1
                 : layout==SE_NDS_LAYOUT_HYBRID_LARGE_TOP||layout==SE_NDS_LAYOUT_HYBRID_LARGE_BOTTOM? 3 : 2;
    CHECK(n==expected,"layout %d: %d screens",layout,n);
    bool seen[2]={false,false};
    for(int i=0;i<n;++i){
      CHECK(p[i].x>=-1e-3f&&p[i].y>=-1e-3f&&p[i].x+p[i].w<=bw+1e-3f&&p[i].y+p[i].h<=bh+1e-3f,
            "layout %d gap %g size %g: screen %d is outside the box",layout,gaps[g],sizes[s],i);
      CHECK(near(p[i].w*192,p[i].h*256),"layout %d: screen %d is not 4:3",layout,i);
      CHECK(near(p[i].w,256)||near(p[i].w,256*sizes[s]),"layout %d: screen %d has width %g",layout,i,p[i].w);
      seen[p[i].screen]=true;
      for(int j=0;j<i;++j){
        CHECK(!overlap(&p[i],&p[j]),"layout %d gap %g size %g: screens %d and %d overlap",layout,gaps[g],sizes[s],i,j);
        CHECK(spacing(&p[i],&p[j])>=gaps[g]-1e-3f,"layout %d gap %g: screens %d and %d are %g apart",
              layout,gaps[g],i,j,spacing(&p[i],&p[j]));
      }
    }
    // Every layout but the single screen ones shows both screens
    if(n>1)CHECK(seen[0]&&seen[1],"layout %d: a screen is missing",layout);
    // The box is as small as it can be: some screen touches each of its edges
    float max_x=0, max_y=0;
    for(int i=0;i<n;++i){max_x=fmaxf(max_x,p[i].x+p[i].w); max_y=fmaxf(max_y,p[i].y+p[i].h);}
    CHECK(near(max_x,bw)&&near(max_y,bh),"layout %d: box %gx%g is larger than the screens",layout,bw,bh);

    // Swap exchanges the screens and keeps the places
    se_screen_place_t q[3];
    se_nds_layout_place(layout,gaps[g],sizes[s],!swap,q,NULL,NULL);
    for(int i=0;i<n;++i){
      CHECK(near(p[i].x,q[i].x)&&near(p[i].y,q[i].y)&&near(p[i].w,q[i].w),"layout %d: swap moved screen %d",layout,i);
      CHECK(p[i].screen!=q[i].screen,"layout %d: swap kept screen %d",layout,i);
    }
  }
}

static void test_single_screens(void){
  se_screen_place_t p[3];
  float bw=0, bh=0;
  CHECK(se_nds_layout_place(SE_NDS_LAYOUT_TOP_ONLY,8,0.5f,false,p,&bw,&bh)==1&&p[0].screen==T,"top only");
  CHECK(near(bw,256)&&near(bh,192)&&near(p[0].w,256),"top only fills the box");
  CHECK(se_nds_layout_place(SE_NDS_LAYOUT_BOTTOM_ONLY,8,0.5f,false,p,&bw,&bh)==1&&p[0].screen==B,"bottom only");
  CHECK(se_nds_layout_place(SE_NDS_LAYOUT_TOP_ONLY,0,0.5f,true,p,NULL,NULL)==1&&p[0].screen==B,"swapped top only");
}

static void test_values(void){
  se_screen_place_t p[3];
  float bw=0, bh=0;
  // Gap between the screens
  se_nds_layout_place(SE_NDS_LAYOUT_VERTICAL,32,0.5f,false,p,&bw,&bh);
  CHECK(near(p[1].y,192+32)&&near(bh,192*2+32),"vertical gap");
  // Hybrid with large small screens: the column is taller than the large screen, which is centered
  se_nds_layout_place(SE_NDS_LAYOUT_HYBRID_LARGE_TOP,10,0.75f,false,p,&bw,&bh);
  CHECK(near(bh,144*2+10)&&near(p[2].y,(bh-192)*0.5f)&&near(bw,256+10+192),"hybrid column %gx%g",bw,bh);
  // Out of range options are clamped
  se_nds_layout_place(SE_NDS_LAYOUT_VERTICAL_LARGE_TOP,-5,0,false,p,&bw,&bh);
  CHECK(near(p[1].w,128)&&near(p[1].y,192),"defaults for 0 and negative options");
  se_nds_layout_place(SE_NDS_LAYOUT_VERTICAL_LARGE_TOP,0,4,false,p,&bw,&bh);
  CHECK(near(p[1].w,256),"small size above 1");
  se_nds_layout_place(SE_NDS_LAYOUT_VERTICAL_LARGE_TOP,0,0.1f,false,p,&bw,&bh);
  CHECK(near(p[1].w,64),"small size below 0.25");
  // Unknown layouts and Auto place nothing
  CHECK(se_nds_layout_place(SE_NDS_LAYOUT_AUTO,0,0.5f,false,p,&bw,&bh)==0,"auto");
  CHECK(se_nds_layout_place(SE_NDS_NUM_LAYOUTS,0,0.5f,false,p,&bw,&bh)==0&&bw==0&&bh==0,"unknown layout");
}

int main(void){
  test_classic_layouts();
  test_all_options();
  test_single_screens();
  test_values();
  if(failures){printf("%d checks failed\n",failures); return 1;}
  printf("All screen layout tests passed\n");
  return 0;
}
