#include "split_probe.h"
uint32_t render_args[5];
void native_dispatch(GuestCpu *frame, unsigned slot) {
    if (slot != 521) return;
    split_probe_call_render(frame, 0x130101);
    frame->r[15] = frame->r[14];
}
