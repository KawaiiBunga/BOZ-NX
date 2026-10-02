#include "net.h"
#include "net_trace.h"
#include "split_probe.h"
#include <assert.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>

static GuestMem memory;
static unsigned char heap[0x10000];
static unsigned alloc_offset=0x8000, connected, accepted, readable;
static uint32_t alloc(uint32_t size) {
    unsigned old=alloc_offset; alloc_offset=(alloc_offset+size+3)&~3u;
    assert(alloc_offset<sizeof heap); return 0x100000+old;
}
static int callback(uint32_t fn,uint32_t a,uint32_t b,uint32_t user,uint32_t *result) {
    assert(a>=3000 && a<3032); assert(user==0xbeef);
    if(fn==0x7777) { uint32_t success; assert(guest_ld32(&memory,b,&success)&&success==0); ++connected; }
    else if(fn==0x8888) ++accepted;
    else if(fn==0x9999) ++readable;
    else assert(0);
    if(result) *result=0;
    return 1;
}
static int32_t call(const char *name,uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t fifth) {
    GuestCpu frame={0}; frame.r[0]=a;frame.r[1]=b;frame.r[2]=c;frame.r[3]=d;
    frame.r[13]=0x101800; guest_st32(&memory,frame.r[13],fifth);
    GuestHleFn function=net_find_hle(name); assert(function);
    function(&frame,&memory,NULL); return (int32_t)frame.r[0];
}
static uint32_t socket_(int udp) {
    int32_t result=call("s3eSocketCreate",udp,2,0,0,0);assert(result>=3000);return result;
}
static void endpoint(uint32_t address) {
    guest_st32(&memory,address+0x88,htonl(0x7f000001));guest_st16(&memory,address+0x9c,0);
}
static void bind_(uint32_t socket,uint32_t address) {
    endpoint(address);assert(call("s3eSocketBind",socket,address,1,0,0)==0);
    assert(call("s3eSocketGetLocalName",socket,address,0,0,0)==0);
    uint32_t port; assert(guest_ld16(&memory,address+0x9c,&port)&&port);
}
int main(void) {
    guest_mem_add(&memory,0x100000,sizeof heap,heap,1);
    split_probe_init(1); NetGlue glue={alloc,callback};net_init(&glue,1);
    uint32_t server=socket_(0), client=socket_(0), address=0x101000;
    bind_(server,address);assert(call("s3eSocketListen",server,2,0,0,0)==0);
    assert(call("s3eSocketAccept",server,0x101200,0x8888,0xbeef,0)==0);
    assert(call("s3eSocketConnect",client,address,0x7777,0xbeef,0)==0);
    for(unsigned i=0;i<1000 && (!connected||!accepted);++i) { net_pump(&memory);usleep(1000); }
    assert(connected==1 && accepted==1);
    uint32_t peer=call("s3eSocketAccept",server,0x101200,0,0,0);assert(peer>=3000);
    assert(call("s3eSocketRecv",peer,0x102100,16,0,0)==-1);
    assert(call("s3eSocketGetError",0,0,0,0,0)==1000);
    const char message[]="0123456789abcdef";
    memcpy(heap+0x2000,message,16);
    assert(call("s3eSocketReadable",peer,0x9999,0xbeef,0,0)==0);
    assert(call("s3eSocketSend",client,0x102000,16,0,0)==16);
    for(unsigned i=0;i<1000&&!readable;++i) { net_pump(&memory);usleep(1000); }
    assert(readable==1);
    assert(call("s3eSocketRecv",peer,0x102100,4,0,0)==4);
    assert(call("s3eSocketRecv",peer,0x102104,12,0,0)==12);
    assert(!memcmp(heap+0x2100,message,16));
    uint32_t udp_receiver=socket_(1),udp_sender=socket_(1);
    bind_(udp_receiver,address);
    assert(call("s3eSocketSendTo",udp_sender,0x102000,16,0,address)==16);
    int32_t got=-1;
    for(unsigned i=0;i<1000&&got<0;++i) {
        got=call("s3eSocketRecvFrom",udp_receiver,0x102200,64,0,0x101200);
        if(got<0) usleep(1000);
    }
    assert(got==16&&!memcmp(heap+0x2200,message,16));
    uint32_t ip;assert(guest_ld32(&memory,0x101200+0x88,&ip)&&ip==htonl(0x7f000001));
    /* A zero-byte UDP packet is a valid datagram, unlike TCP EOF. */
    assert(call("s3eSocketSendTo",udp_sender,0,0,0,address)==0);
    assert(call("s3eSocketRecvFrom",udp_receiver,0x102200,64,0,0x101200)==0);
    for(unsigned i=0;i<5;++i) {
        uint32_t sockets[]={server,client,peer,udp_receiver,udp_sender};
        assert(call("s3eSocketClose",sockets[i],0,0,0,0)==0);
    }
    uint32_t reused=socket_(1);assert(reused==server);
    assert(call("s3eSocketClose",reused,0,0,0,0)==0);
    FILE *out=tmpfile();assert(out);net_trace_write(out);rewind(out);
    char report[30000];size_t n=fread(report,1,sizeof report-1,out);report[n]=0;fclose(out);
    assert(strstr(report,"op=accept ")&&strstr(report,"op=connected "));
    assert(strstr(report,"op=read-ready ")&&strstr(report,"op=recvfrom "));
    assert(strstr(report,"handle=3000 generation=3 transport=udp"));
    assert(strstr(report,"want=16 result=-1")&&strstr(report,"want=4 result=4"));
    puts("PASS: production socket SDK over loopback TCP/UDP with trace: connect/accept/read callbacks, partial stream reads, would-block, datagram endpoints/zero length and handle reuse (mDNS not tested)");
}
