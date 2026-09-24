#include "core/state.h"
#include "net/socket/socket.h"

// clang-format off
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
// clang-format on

typedef struct RDNetSocket {
    int _unused;
} RDNetSocket;

typedef struct RDSocket {
    SOCKET fd;
    RDSocketKind kind;
} RDSocket;

static bool _rd_net_socket_wait_ready(SOCKET fd, SHORT events, u32 timeout_ms) {
    WSAPOLLFD pfd = {.fd = fd, .events = events};
    int t = timeout_ms ? (int)timeout_ms : -1;

    int r = WSAPoll(&pfd, 1, t);
    if(r <= 0) return false; // timeout or error

    return (pfd.revents & events) && !(pfd.revents & (POLLERR | POLLHUP));
}

static void _rd_net_socket_set_nonblocking(SOCKET fd, bool enable) {
    u_long mode = enable ? 1 : 0;
    ioctlsocket(fd, FIONBIO, &mode);
}

static void _rd_net_socket_set_send_timeout(SOCKET fd, u32 timeout_ms) {
    DWORD tv = timeout_ms;
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
}

void _rd_net_socket_init(void) {
    if(rd_i_state.net_socket) return;

    WSADATA wsadata;
    int rc = WSAStartup(MAKEWORD(2, 2), &wsadata);

    if(rc != 0) {
        RD_LOG_FAIL("WSAStartup failed (%d)", rc);
        return;
    }

    rd_i_state.net_socket = rd_alloc0(1, sizeof(RDNetSocket));
}

void _rd_net_socket_deinit(void) {
    if(!rd_i_state.net_socket) return;

    WSACleanup();
    rd_free(rd_i_state.net_socket);
    rd_i_state.net_socket = NULL;
}

RDSocket* _rd_net_socket_connect(RDSocketKind kind, const char* host, u16 port,
                                 u32 timeout_ms) {
    if(!rd_i_state.net_socket) return NULL;

    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = (kind == RD_SOCKET_TCP) ? SOCK_STREAM : SOCK_DGRAM;

    char portstr[6];
    snprintf(portstr, sizeof(portstr), "%u", port);

    struct addrinfo* res = NULL;
    if(getaddrinfo(host, portstr, &hints, &res) != 0 || !res) {
        RD_LOG_FAIL("socket: cannot resolve '%s'", host);
        return NULL;
    }

    SOCKET fd = INVALID_SOCKET;

    for(struct addrinfo* it = res; it; it = it->ai_next) {
        fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if(fd == INVALID_SOCKET) continue;

        _rd_net_socket_set_nonblocking(fd, true);
        int rc = connect(fd, it->ai_addr, (int)it->ai_addrlen);

        if(rc == 0) break; // connected immediately (common for UDP)

        if(WSAGetLastError() == WSAEWOULDBLOCK) {
            if(_rd_net_socket_wait_ready(fd, POLLOUT, timeout_ms)) {
                int err = 0;
                int len = sizeof(err);
                getsockopt(fd, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
                if(err == 0) break; // connected
            }
        }

        closesocket(fd);
        fd = INVALID_SOCKET;
    }

    freeaddrinfo(res);

    if(fd == INVALID_SOCKET) {
        RD_LOG_FAIL("socket: connect to '%s:%u' failed", host, port);
        return NULL;
    }

    _rd_net_socket_set_nonblocking(
        fd, false); // send/recv below manage their own waits

    RDSocket* s = rd_alloc(sizeof(RDSocket));
    s->fd = fd;
    s->kind = kind;
    return s;
}

RDSocketResult _rd_net_socket_send(RDSocket* s, const void* data, usize n,
                                   u32 timeout_ms) {
    RDSocketResult r = {.ok = false, .length = 0};
    if(!s) return r;

    if(!_rd_net_socket_wait_ready(s->fd, POLLOUT, timeout_ms))
        return r; // layer 1: buffer wait

    _rd_net_socket_set_send_timeout(s->fd,
                                    timeout_ms); // layer 2: syscall-level bound
    int sent = send(s->fd, (const char*)data, (int)n, 0);
    if(sent == SOCKET_ERROR) return r;

    r.ok = true;
    r.length = (usize)sent;
    return r;
}

RDSocketResult _rd_net_socket_recv(RDSocket* s, RDScratchBuffer* reply) {
    RDCharVect* buf = &reply->impl;
    RDSocketResult r = {.ok = false, .length = 0};
    if(!s || vect_is_empty(buf)) return r;

    int got = recv(s->fd, buf->data, (int)vect_length(buf), 0);

    if(got > 0) {
        r.ok = true;
        r.length = (usize)got;
    }

    // got == 0: peer closed the connection - ok stays false
    // got == SOCKET_ERROR: socket error - same

    return r;
}

void _rd_net_socket_close(RDSocket* s) {
    if(!s) return;
    closesocket(s->fd);
    rd_free(s);
}
