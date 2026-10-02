/* Calls the production controller helper, the real camera updater and the
 * real camera setter. Game image and globals are supplied by the PC test. */
#include "camera_controller.h"
CameraController camera_state;
GuestMem camera_memory = {.region={
    {0x800000,0x600000,(unsigned char *)0x800000,1},
    {0x300000,0x20000,(unsigned char *)0x300000,1}},.count=2};
unsigned original_calls, setter_calls;
static uint32_t call(void *user,uint32_t fn,uint32_t a,uint32_t b) {
    (void)user;
    if (fn==0x130101) ++original_calls; else ++setter_calls;
    return ((uint32_t (*)(uint32_t,uint32_t))(uintptr_t)fn)(a,b);
}
void native_dispatch(GuestCpu *frame,unsigned slot) {
    if(slot!=520) return;
    frame->r[0]=camera_controller_update(&camera_state,&camera_memory,frame->r[0],
        0x130101,0x8b95cd,call,0);
    frame->r[15]=frame->r[14];
}
