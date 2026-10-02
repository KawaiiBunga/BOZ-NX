#include "net_trace.h"
#include <string.h>

/* Separate rings keep frequent I/O from erasing join/leave events. Writes are
 * guest-thread only; no SD writes or allocations in the packet path. */
#define LIFECYCLE_EVENTS 128u
#define IO_EVENTS 256u
typedef struct {
    uint64_t sequence;
    enum NetTraceOp op;
    uint32_t handle, generation, ip, requested;
    int32_t result;
    unsigned port;
    int udp, error;
} Event;
static struct {
    int enabled;
    uint64_t sequence, life_count, io_count;
    Event life[LIFECYCLE_EVENTS], io[IO_EVENTS];
} trace;
static const char *const names[] = {
    "create", "close", "bind", "listen", "accept", "connect", "connected",
    "send", "sendto", "recv", "recvfrom", "read-ready", "write-ready",
    "accept-ready", "discovery-start", "discovery-publish", "discovery-stop"
};
void net_trace_init(int enabled) {
    memset(&trace, 0, sizeof trace);
    trace.enabled = !!enabled;
}
void net_trace_record(enum NetTraceOp op, uint32_t handle, uint32_t generation,
                      int udp, uint32_t ip, unsigned port, uint32_t requested,
                      int32_t result, int error) {
    if (!trace.enabled || (unsigned)op >= sizeof names / sizeof names[0]) return;
    int io = op >= NET_TRACE_SEND && op <= NET_TRACE_ACCEPT_READY;
    uint64_t *count = io ? &trace.io_count : &trace.life_count;
    Event *ring = io ? trace.io : trace.life;
    unsigned capacity = io ? IO_EVENTS : LIFECYCLE_EVENTS;
    Event *event = &ring[(*count)++ % capacity];
    *event = (Event){++trace.sequence, op, handle, generation, ip, requested,
                     result, port, udp, error};
}
void net_trace_write(FILE *out) {
    uint64_t l = trace.life_count > LIFECYCLE_EVENTS ? trace.life_count-LIFECYCLE_EVENTS : 0;
    uint64_t i = trace.io_count > IO_EVENTS ? trace.io_count-IO_EVENTS : 0;
    fprintf(out, "network-trace lifecycle-total=%llu io-total=%llu lifecycle-overwritten=%llu io-overwritten=%llu\n",
            (unsigned long long)trace.life_count, (unsigned long long)trace.io_count,
            (unsigned long long)l, (unsigned long long)i);
    /* Merge retained events by sequence so TCP accept/connect/readiness and
     * subsequent packets can be reconstructed in their observed order. */
    while (l < trace.life_count || i < trace.io_count) {
        Event *a = l < trace.life_count ? &trace.life[l % LIFECYCLE_EVENTS] : NULL;
        Event *b = i < trace.io_count ? &trace.io[i % IO_EVENTS] : NULL;
        Event *e;
        if (a && (!b || a->sequence < b->sequence)) { e = a; ++l; }
        else { e = b; ++i; }
        fprintf(out, "net seq=%llu op=%s handle=%u generation=%u transport=%s peer=%u.%u.%u.%u:%u want=%u result=%d errno=%d\n",
                (unsigned long long)e->sequence, names[e->op], e->handle, e->generation,
                e->udp ? "udp" : "tcp", e->ip >> 24, (e->ip >> 16) & 255,
                (e->ip >> 8) & 255, e->ip & 255, e->port, e->requested, e->result, e->error);
    }
}
