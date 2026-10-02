/* Time-based cursor movement is independent of the game's FPS. Toggle edges
 * and a release guard prevent a cursor click from becoming a shot on exit. */
#include <switch.h>
#include <math.h>
#include "cursor.h"
static int active, combo_was, guard, pressed;
static float x=640,y=360;
static uint64_t previous;
static unsigned last_frame=~0u;
void cursor_update(uint64_t held,int sx,int sy,uint64_t tick,uint64_t freq,unsigned frame) {
  if (frame == last_frame) return;
  last_frame=frame;
  const uint64_t combo=HidNpadButton_L|HidNpadButton_R|HidNpadButton_Minus;
  int chord=(held&combo)==combo;
  if (chord && !combo_was) { active=!active; guard=1; pressed=0; }
  combo_was=chord;
  const uint64_t consumed=combo|HidNpadButton_A|HidNpadButton_ZR;
  if (guard && !(held&consumed)) guard=0;
  float dt=previous && freq ? (float)(tick-previous)/(float)freq : 0;
  previous=tick;
  if (dt>0.05f) dt=0.05f;
  if (!active || guard) { pressed=0; return; }
  float fx=(float)sx/32767.f, fy=(float)sy/32767.f;
  float radius=sqrtf(fx*fx+fy*fy);
  if (radius>0.18f) {
    float speed=(held&HidNpadButton_ZL)?180.f:800.f;
    float mag=(radius-0.18f)/0.82f;
    x+=fx/radius*mag*speed*dt; y-=fy/radius*mag*speed*dt;
    if(x<0)x=0;
    if(x>1279)x=1279;
    if(y<0)y=0;
    if(y>719)y=719;
  }
  pressed=(held&(HidNpadButton_A|HidNpadButton_ZR))!=0;
}
int cursor_active(void) { return active; }
int cursor_consumes(void) { return active || guard || combo_was; }
int cursor_contact(int *px,int *py) { *px=(int)x; *py=(int)y; return active&&pressed; }
int cursor_position(int *px,int *py) { *px=(int)x; *py=(int)y; return active; }
