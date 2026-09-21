#include "net/socket/socket.h"
#include <redasm/common.h>

void _rd_net_socket_init(void) {}
void _rd_net_socket_deinit(void) {}

RDSocket* _rd_net_socket_connect(RDSocketKind kind, const char* host, u16 port,
                                 u32 timeout_ms) {
    RD_UNUSED(kind);
    RD_UNUSED(host);
    RD_UNUSED(port);
    RD_UNUSED(timeout_ms);
    return NULL;
}

RDSocketResult _rd_net_socket_send(RDSocket* s, const void* data, usize n,
                                   u32 timeout_ms) {
    RD_UNUSED(s);
    RD_UNUSED(data);
    RD_UNUSED(n);
    RD_UNUSED(timeout_ms);
    return (RDSocketResult){.ok = false, .length = 0};
}

RDSocketResult _rd_net_socket_recv(RDSocket* s, RDScratchBuffer* reply) {
    RD_UNUSED(s);
    RD_UNUSED(reply);
    return (RDSocketResult){.ok = false, .length = 0};
}

void _rd_net_socket_close(RDSocket* s) { RD_UNUSED(s); }
