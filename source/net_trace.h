#ifndef BOZ_NET_TRACE_H
#define BOZ_NET_TRACE_H
#include <stdint.h>
#include <stdio.h>
enum NetTraceOp {
    NET_TRACE_CREATE, NET_TRACE_CLOSE, NET_TRACE_BIND, NET_TRACE_LISTEN,
    NET_TRACE_ACCEPT, NET_TRACE_CONNECT, NET_TRACE_CONNECTED,
    NET_TRACE_SEND, NET_TRACE_SENDTO, NET_TRACE_RECV, NET_TRACE_RECVFROM,
    NET_TRACE_READ_READY, NET_TRACE_WRITE_READY, NET_TRACE_ACCEPT_READY,
    NET_TRACE_DISCOVERY_START, NET_TRACE_DISCOVERY_PUBLISH, NET_TRACE_DISCOVERY_STOP
};
/* Endpoints are host-order IPv4/port; generation disambiguates reused handles.
 * No packet bytes, account credentials or player names are recorded. */
void net_trace_init(int enabled);
void net_trace_record(enum NetTraceOp op, uint32_t handle, uint32_t generation,
                      int udp, uint32_t ip, unsigned port, uint32_t requested,
                      int32_t result, int error);
void net_trace_write(FILE *out);
#endif
