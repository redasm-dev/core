#include <redasm/common.h>
#include <redasm/net/socket.h>
#include <redasm/support/logging.h>

#if defined(RD_HAS_NETWORK)
#include "net/socket/socket.h"
#include <redasm/net/net.h>
#endif

RDSocket* rd_net_socket_connect(RDSocketKind kind, const char* host, u16 port,
                                u32 timeout_ms) {
#if defined(RD_HAS_NETWORK)
    if(!rd_net_is_enabled()) return NULL;
    return _rd_net_socket_connect(kind, host, port, timeout_ms);
#else
    RD_UNUSED(kind);
    RD_UNUSED(host);
    RD_UNUSED(port);
    RD_UNUSED(timeout_ms);
    return NULL;
#endif
}

RDSocket* rd_net_socket_listen(RDSocketKind kind, const char* host, u16 port) {
#if defined(RD_HAS_NETWORK)
    if(!rd_net_is_enabled()) return NULL;
    return _rd_net_socket_listen(kind, host, port);
#else
    RD_UNUSED(kind);
    RD_UNUSED(host);
    RD_UNUSED(port);
    return NULL;
#endif
}

RDSocket* rd_net_socket_accept(RDSocket* s, u32 timeout_ms) {
#if defined(RD_HAS_NETWORK)
    if(!rd_net_is_enabled()) return NULL;
    return _rd_net_socket_accept(s, timeout_ms);
#else
    RD_UNUSED(s);
    RD_UNUSED(timeout_ms);
    return NULL;
#endif
}

RDSocketResult rd_net_socket_send(RDSocket* s, const void* data, usize n,
                                  u32 timeout_ms) {
#if defined(RD_HAS_NETWORK)
    return _rd_net_socket_send(s, data, n, timeout_ms);
#else
    RD_UNUSED(s);
    RD_UNUSED(data);
    RD_UNUSED(n);
    RD_UNUSED(timeout_ms);
    return (RDSocketResult){.status = RD_SOCKET_ERROR, .length = 0};
#endif
}

RDSocketResult rd_net_socket_recv(RDSocket* s, RDScratchBuffer* reply,
                                  u32 timeout_ms) {
#if defined(RD_HAS_NETWORK)
    return _rd_net_socket_recv(s, reply, timeout_ms);
#else
    RD_UNUSED(s);
    RD_UNUSED(reply);
    RD_UNUSED(timeout_ms);
    return (RDSocketResult){.status = RD_SOCKET_ERROR, .length = 0};
#endif
}

void rd_net_socket_close(RDSocket* s) {
#if defined(RD_HAS_NETWORK)
    _rd_net_socket_close(s);
#else
    RD_UNUSED(s);
#endif
}
