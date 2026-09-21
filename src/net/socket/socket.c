#include <redasm/common.h>
#include <redasm/net/socket.h>
#include <redasm/support/logging.h>

#if defined(RD_HAS_NETWORK)
#include "net/socket/socket.h"
#include <redasm/net/net.h>
#endif

RDSocket* rd_socket_connect(RDSocketKind kind, const char* host, u16 port,
                            u32 timeout_ms) {
#if defined(RD_HAS_NETWORK)
    if(!rd_net_is_enabled()) return NULL;
    return _rd_net_socket_connect(kind, host, port, timeout_ms);
#else
    return NULL;
#endif
}

RDSocketResult rd_socket_send(RDSocket* s, const void* data, usize n,
                              u32 timeout_ms) {
#if defined(RD_HAS_NETWORK)
    return _rd_net_socket_send(s, data, n, timeout_ms);
#else
    return (RDSocketResult){.ok = false, .length = 0};
#endif
}

RDSocketResult rd_socket_recv(RDSocket* s, RDScratchBuffer* reply) {
#if defined(RD_HAS_NETWORK)
    return _rd_net_socket_recv(s, reply);
#else
    return (RDSocketResult){.ok = false, .length = 0};
#endif
}

void rd_socket_close(RDSocket* s) {
#if defined(RD_HAS_NETWORK)
    _rd_net_socket_close(s);
#else
    RD_UNUSED(s);
#endif
}
