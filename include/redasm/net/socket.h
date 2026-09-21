#pragma once

#include <redasm/config.h>
#include <redasm/support/scratch.h>

typedef struct RDSocket RDSocket;

typedef enum {
    RD_SOCKET_TCP,
    RD_SOCKET_UDP,
} RDSocketKind;

typedef struct RDSocketResult {
    bool ok;
    usize length;
} RDSocketResult;

// clang-format off
RD_API RDSocket* rd_socket_connect(RDSocketKind kind, const char* host, u16 port, u32 timeout_ms);
RD_API RDSocketResult rd_socket_send(RDSocket* s, const void* data, usize n, u32 timeout_ms);
RD_API RDSocketResult rd_socket_recv(RDSocket* s, RDScratchBuffer* reply);
RD_API void rd_socket_close(RDSocket* s);
// clang-format on
