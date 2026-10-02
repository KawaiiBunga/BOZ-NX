#ifndef BOZ_CURSOR_H
#define BOZ_CURSOR_H
#include <stdint.h>
void cursor_update(uint64_t held,int sx,int sy,uint64_t tick,uint64_t freq,unsigned frame);
int cursor_active(void);
int cursor_consumes(void);
int cursor_contact(int *x,int *y);
int cursor_position(int *x,int *y);
#endif
