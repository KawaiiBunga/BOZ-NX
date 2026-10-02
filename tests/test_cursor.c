#include <assert.h>
#include <stdio.h>
#include "switch.h"
#include "cursor.h"
int main(void) {
  const uint64_t combo=HidNpadButton_L|HidNpadButton_R|HidNpadButton_Minus;
  int x,y;
  cursor_update(combo,0,0,1000000,1000000,0);
  assert(cursor_active() && cursor_consumes() && !cursor_contact(&x,&y));
  cursor_update(combo,0,0,1016000,1000000,1); assert(cursor_active());
  cursor_update(0,0,0,1032000,1000000,2);
  cursor_update(HidNpadButton_A,32767,0,1048000,1000000,3);
  assert(cursor_contact(&x,&y) && x>640 && y==360);
  int before=x;
  cursor_update(HidNpadButton_A,32767,0,1064000,1000000,3);
  cursor_position(&x,&y); assert(x==before); /* repeated SDK poll is idempotent */
  cursor_update(0,0,0,1080000,1000000,4); assert(!cursor_contact(&x,&y));
  cursor_update(combo|HidNpadButton_ZR,0,0,1096000,1000000,5);
  assert(!cursor_active() && cursor_consumes());
  cursor_update(HidNpadButton_ZR,0,0,1112000,1000000,6); assert(cursor_consumes());
  cursor_update(0,0,0,1128000,1000000,7); assert(!cursor_consumes());
  cursor_update(combo,0,0,1144000,1000000,8); assert(cursor_active());
  cursor_update(0,0,0,1160000,1000000,9);
  for(unsigned f=10;f<200;f++)cursor_update(0,32767,32767,1160000+(f-9)*50000,1000000,f);
  cursor_position(&x,&y);assert(x==1279 && y==0);
  puts("PASS: cursor chord edge, drag, polling, release guard, movement and clamping");
}
