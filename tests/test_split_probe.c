#include "split_probe.h"
#include "net_trace.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

static void word(unsigned char *heap, unsigned offset, uint32_t value) {
    memcpy(heap + offset, &value, 4);
}
int main(void) {
    unsigned char heap[0x200] = {0}, before[0x200];
    GuestMem mem = {0};
    guest_mem_add(&mem, 0x300000, sizeof heap, heap, 1);
    word(heap, 0, 0x800000 + 0x4069d8); word(heap, 4, 0x1234);
    word(heap, 0x48, 0x305000); word(heap, 0x50, 0x310000); word(heap, 0x60, 0x315000);
    word(heap, 0x78, 0x320000); word(heap, 0x80, 0x325000); word(heap, 0x1e0, 0x330000);
    memcpy(before, heap, sizeof heap);
    SplitObject object;
    assert(split_probe_identify(&mem, 0x800000, 0x300000, sizeof heap, &object));
    assert(object.kind == SPLIT_WEAPON_MANAGER && object.identity == 0x1234);
    assert(object.player_entity == 0x305000 && object.transform == 0x310000);
    assert(object.player == 0x315000 && object.camera == 0x320000 && object.weapon == 0x330000);
    assert(object.camera_transform == 0x325000);
    assert(split_probe_identify(&mem, 0x800000, 0x300000, 0x50, &object));
    assert(object.player_entity == 0x305000 && !object.transform && !object.player && !object.camera_transform);
    assert(split_probe_identify(&mem, 0x800000, 0x300000, 8, &object));
    assert(!object.camera && !object.player && !object.weapon && !object.player_entity && !object.camera_transform);
    assert(!split_probe_identify(&mem, 0x800000, 0x300000, 4, &object));
    assert(!split_probe_identify(&mem, 0x800000, 0x3001ff, 8, &object));
    assert(!split_probe_identify(&mem, 0x800000, 0xfffffffc, 8, &object));
    assert(!split_probe_identify(&mem, 0x900000, 0x300000, sizeof heap, &object));
    assert(!memcmp(before, heap, sizeof heap));
    uint32_t vtables[] = {0x400798,0x401968,0x4069d8,0x406b10,0x3fa678,0x3fd1c8,0x3fa418,0x3e6290};
    for (unsigned i = 0; i < 8; ++i) {
        word(heap, 0, 0x800000 + vtables[i]);
        assert(split_probe_identify(&mem, 0x800000, 0x300000, sizeof heap, &object));
        assert(object.kind == (enum SplitObjectKind)(i + 1));
    }
    word(heap, 0, 0x800000 + vtables[0]); word(heap, 0x0c, 0x340000); word(heap, 0x168, 0x350000);
    assert(split_probe_identify(&mem, 0x800000, 0x300000, sizeof heap, &object));
    assert(object.entity == 0x340000 && object.transform == 0x350000);
    word(heap, 0, 0x800000 + vtables[1]); word(heap, 0xa8, 0x360000);
    assert(split_probe_identify(&mem, 0x800000, 0x300000, sizeof heap, &object));
    assert(object.transform == 0x360000);
    SplitPad a = {1, 0x400, 100, 200, 300, 400}, b = {1,0x800,-100,-200,-300,-400};
    split_probe_init(0); split_probe_set_pad(0, &a);
    assert(!split_probe_pad(0)->buttons);
    split_probe_init(1); split_probe_set_pad(0,&a); split_probe_set_pad(1,&b);
    assert(split_probe_pad(0)->buttons == a.buttons && split_probe_pad(0)->lx == a.lx);
    assert(split_probe_pad(1)->buttons == b.buttons && split_probe_pad(1)->rx == b.rx);
    b.connected = 0; split_probe_set_pad(1,&b);
    assert(!split_probe_pad(1)->buttons && !split_probe_pad(1)->lx);
    assert(split_probe_pad(2) == NULL);

    net_trace_init(0);
    net_trace_record(NET_TRACE_CREATE,3000,1,0,0,0,0,0,0);
    FILE *f = tmpfile(); assert(f); net_trace_write(f); rewind(f);
    char output[100000]; size_t n=fread(output,1,sizeof output-1,f); output[n]=0; fclose(f);
    assert(strstr(output,"lifecycle-total=0 io-total=0"));
    net_trace_init(1);
    net_trace_record(NET_TRACE_CREATE,3000,1,0,0x7f000001,3074,0,0,0);
    net_trace_record(NET_TRACE_SEND,3000,1,0,0x7f000001,3074,100,50,0);
    net_trace_record(NET_TRACE_CLOSE,3000,1,0,0,0,0,0,0);
    net_trace_record(NET_TRACE_CREATE,3000,3,1,0,3478,0,0,0);
    for (unsigned i=0;i<300;++i) net_trace_record(NET_TRACE_RECVFROM,3000,3,1,0,0,100,-1,11);
    f=tmpfile(); assert(f); net_trace_write(f); rewind(f);
    n=fread(output,1,sizeof output-1,f); output[n]=0; fclose(f);
    assert(strstr(output,"io-overwritten=45"));
    assert(strstr(output,"seq=1 op=create handle=3000 generation=1 transport=tcp peer=127.0.0.1:3074"));
    assert(strstr(output,"seq=4 op=create handle=3000 generation=3 transport=udp"));
    assert(strstr(output,"want=100 result=-1 errno=11"));
    /* Lifecycle history survives I/O overflow; merged sequence stays ordered. */
    char *scan=output; unsigned long long previous=0;
    while ((scan=strstr(scan,"net seq="))) {
        unsigned long long sequence=strtoull(scan+8,NULL,10);
        assert(sequence>previous); previous=sequence; ++scan;
    }
    puts("PASS: split probe controller isolation/disconnect, bounded read-only object inventory, network generations and separate trace rings");
}
