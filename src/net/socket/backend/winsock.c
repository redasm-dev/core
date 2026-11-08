#include "core/state.h"
#include "net/socket/socket.h"
#include <stdio.h>

// clang-format off
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
// clang-format on

#if !defined(SIO_UDP_CONNRESET)
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

typedef struct RDNetSocket {
    int _unused;
} RDNetSocket;

typedef struct RDSocket {
    SOCKET fd;
    RDSocketKind kind;
    bool is_server;               // created by _rd_net_socket_listen
    struct sockaddr_storage peer; // UDP server: sender of the last datagram
    int peer_len;
} RDSocket;

static RDSocket* _rd_net_socket_new(SOCKET fd, RDSocketKind kind,
                                    bool is_server) {
    RDSocket* s = rd_alloc0(1, sizeof(RDSocket));
    s->fd = fd;
    s->kind = kind;
    s->is_server = is_server;
    return s;
}

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

    return _rd_net_socket_new(fd, kind, false);
}

RDSocket* _rd_net_socket_listen(RDSocketKind kind, const char* host, u16 port) {
    if(!rd_i_state.net_socket || !host || !port) return NULL;

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

        // unlike POSIX, SO_REUSEADDR here lets others hijack the port
        BOOL on = TRUE;
        setsockopt(fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&on,
                   sizeof(on));

        if(bind(fd, it->ai_addr, (int)it->ai_addrlen) == 0 &&
           (kind != RD_SOCKET_TCP || listen(fd, SOMAXCONN) == 0))
            break;

        closesocket(fd);
        fd = INVALID_SOCKET;
    }

    freeaddrinfo(res);

    if(fd == INVALID_SOCKET) {
        RD_LOG_FAIL("socket: listen on '%s:%u' failed", host, port);
        return NULL;
    }

    if(kind == RD_SOCKET_TCP) {
        // a client can vanish between poll and accept, which must not block
        _rd_net_socket_set_nonblocking(fd, true);
    }
    else {
        // otherwise ICMP port unreachable surfaces as a recvfrom error
        BOOL report = FALSE;
        DWORD bytes = 0;
        WSAIoctl(fd, SIO_UDP_CONNRESET, &report, sizeof(report), NULL, 0,
                 &bytes, NULL, NULL);
    }

    return _rd_net_socket_new(fd, kind, true);
}

RDSocket* _rd_net_socket_accept(RDSocket* s, u32 timeout_ms) {
    if(!s || !s->is_server || s->kind != RD_SOCKET_TCP) return NULL;

    for(;;) {
        if(!_rd_net_socket_wait_ready(s->fd, POLLRDNORM, timeout_ms))
            return NULL;

        SOCKET fd = accept(s->fd, NULL, NULL);

        if(fd != INVALID_SOCKET) {
            // accepted sockets inherit the listener's non-blocking mode
            _rd_net_socket_set_nonblocking(fd, false);
            return _rd_net_socket_new(fd, RD_SOCKET_TCP, false);
        }

        // client gone before accept: keep waiting (timeout restarts)
        int err = WSAGetLastError();
        if(err != WSAEWOULDBLOCK && err != WSAECONNRESET) return NULL;
    }
}

RDSocketResult _rd_net_socket_send(RDSocket* s, const void* data, usize n,
                                   u32 timeout_ms) {
    RDSocketResult r = {.ok = false, .length = 0};
    if(!s) return r;

    // TCP listeners have no peer, UDP servers need a datagram to reply to
    if(s->is_server && (s->kind == RD_SOCKET_TCP || !s->peer_len)) return r;

    if(!_rd_net_socket_wait_ready(s->fd, POLLOUT, timeout_ms))
        return r; // layer 1: buffer wait

    _rd_net_socket_set_send_timeout(s->fd,
                                    timeout_ms); // layer 2: syscall-level bound

    int sent = s->is_server
                   ? sendto(s->fd, (const char*)data, (int)n, 0,
                            (const struct sockaddr*)&s->peer, s->peer_len)
                   : send(s->fd, (const char*)data, (int)n, 0);
    if(sent == SOCKET_ERROR) return r;

    r.ok = true;
    r.length = (usize)sent;
    return r;
}

RDSocketResult _rd_net_socket_recv(RDSocket* s, RDScratchBuffer* reply) {
    RDCharVect* buf = &reply->impl;
    RDSocketResult r = {.ok = false, .length = 0};
    if(!s || vect_is_empty(buf)) return r;
    if(s->is_server && s->kind == RD_SOCKET_TCP) return r;

    if(s->is_server) {
        struct sockaddr_storage peer;
        int peer_len = sizeof(peer);
        int cap = (int)vect_length(buf);

        int n = recvfrom(s->fd, buf->data, cap, 0, (struct sockaddr*)&peer,
                         &peer_len);

        // oversized datagram: truncate like POSIX does instead of failing
        if(n == SOCKET_ERROR && WSAGetLastError() == WSAEMSGSIZE) n = cap;
        if(n == SOCKET_ERROR) return r;

        // n == 0 is a valid empty datagram, not a closed connection
        s->peer = peer;
        s->peer_len = peer_len;
        r.ok = true;
        r.length = (usize)n;
        return r;
    }

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
