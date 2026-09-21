#pragma once

#include <redasm/net/socket.h>

typedef struct RDNetSocket RDNetSocket;

void _rd_net_socket_init(void);
void _rd_net_socket_deinit(void);

RDSocket* _rd_net_socket_connect(RDSocketKind kind, const char* host, u16 port,
                                 u32 timeout_ms);

RDSocketResult _rd_net_socket_send(RDSocket* s, const void* data, usize n,
                                   u32 timeout_ms);

RDSocketResult _rd_net_socket_recv(RDSocket* s, RDScratchBuffer* reply);
void _rd_net_socket_close(RDSocket* s);
