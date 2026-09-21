#include "net/socket/socket.h"
#include "support/containers.h"
#include "support/scratch.h"
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <redasm/allocator.h>
#include <redasm/support/logging.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct RDSocket {
    int fd;
    RDSocketKind kind;
} RDSocket;

static bool _rd_net_socket_wait_ready(int fd, short events, u32 timeout_ms) {
    struct pollfd pfd = {.fd = fd, .events = events};
    int t = timeout_ms ? (int)timeout_ms : -1;

    for(;;) {
        int r = poll(&pfd, 1, t);
        if(r > 0)
            return (pfd.revents & (events | POLLERR | POLLHUP)) &&
                   !(pfd.revents & (POLLERR | POLLHUP));
        if(r == 0) return false; // timeout
        if(errno != EINTR) return false;
        // EINTR: retry (this does not shrink the remaining timeout)
    }
}

static void _rd_net_socket_set_send_timeout(int fd, u32 timeout_ms) {
    struct timeval tv = {
        .tv_sec = timeout_ms / 1000,
        .tv_usec = (useconds_t)((timeout_ms % 1000) * 1000),
    };

    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

static void _rd_net_socket_set_nonblocking(int fd, bool enable) {
    int flags = fcntl(fd, F_GETFL, 0);
    if(flags < 0) return;
    fcntl(fd, F_SETFL, enable ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK));
}

void _rd_net_socket_init(void) {}
void _rd_net_socket_deinit(void) {}

RDSocket* _rd_net_socket_connect(RDSocketKind kind, const char* host, u16 port,
                                 u32 timeout_ms) {
    if(!host || !port) return NULL;

    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = (kind == RD_SOCKET_TCP) ? SOCK_STREAM : SOCK_DGRAM,
    };

    char portstr[6];
    snprintf(portstr, sizeof(portstr), "%u", port);

    struct addrinfo* res = NULL;
    if(getaddrinfo(host, portstr, &hints, &res) != 0 || !res) {
        RD_LOG_FAIL("socket: cannot resolve '%s'", host);
        return NULL;
    }

    int fd = -1;
    struct addrinfo* it = NULL;

    for(it = res; it; it = it->ai_next) {
        fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if(fd < 0) continue;

        _rd_net_socket_set_nonblocking(fd, true);
        int rc = connect(fd, it->ai_addr, it->ai_addrlen);
        if(rc == 0) break; // connected immediately (common for UDP)

        if(errno == EINPROGRESS) {
            if(_rd_net_socket_wait_ready(fd, POLLOUT, timeout_ms)) {
                int err = 0;
                socklen_t len = sizeof(err);
                getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
                if(err == 0) break; // connected
            }
        }

        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);

    if(fd < 0) {
        RD_LOG_FAIL("socket: connect to '%s:%u' failed", host, port);
        return NULL;
    }

    // send/recv below manage their own waits via poll
    _rd_net_socket_set_nonblocking(fd, false);

    RDSocket* s = rd_alloc(sizeof(RDSocket));
    s->fd = fd;
    s->kind = kind;
    return s;
}

RDSocketResult _rd_net_socket_send(RDSocket* s, const void* data, usize n,
                                   u32 timeout_ms) {
    RDSocketResult r = {.ok = false, .length = 0};
    if(!s || !data || !n) return r;
    if(!_rd_net_socket_wait_ready(s->fd, POLLOUT, timeout_ms)) return r;

    _rd_net_socket_set_send_timeout(s->fd, timeout_ms);
    ssize_t sent = send(s->fd, data, n, 0);
    if(sent < 0) return r;

    r.ok = true;
    r.length = (usize)sent;
    return r;
}

RDSocketResult _rd_net_socket_recv(RDSocket* s, RDScratchBuffer* reply) {
    RDCharVect* buf = &reply->impl;

    RDSocketResult r = {.ok = false, .length = 0};
    if(!s || vect_is_empty(buf)) return r;

    ssize_t got = recv(s->fd, buf->data, vect_length(buf), 0);
    // got == 0: peer closed the connection - ok stays false, n stays 0
    // got < 0: socket error - same

    if(got > 0) {
        r.ok = true;
        r.length = (usize)got;
    }

    return r;
}

void _rd_net_socket_close(RDSocket* s) {
    if(!s) return;
    close(s->fd);
    rd_free(s);
}
