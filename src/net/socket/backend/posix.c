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

#if !defined(MSG_NOSIGNAL)
#define MSG_NOSIGNAL 0 // SO_NOSIGPIPE is set in _rd_net_socket_new instead
#endif

typedef struct RDSocket {
    int fd;
    RDSocketKind kind;
    bool is_server;
    struct sockaddr_storage peer;
    socklen_t peer_len;
} RDSocket;

static RDSocket* _rd_net_socket_new(int fd, RDSocketKind kind, bool is_server) {
#if defined(SO_NOSIGPIPE)
    int on = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif

    RDSocket* s = rd_alloc0(1, sizeof(RDSocket));
    s->fd = fd;
    s->kind = kind;
    s->is_server = is_server;
    return s;
}

static bool _rd_net_socket_poll(int fd, short events, u32 timeout_ms,
                                short* revents) {
    struct pollfd pfd = {.fd = fd, .events = events};
    int t = timeout_ms ? (int)timeout_ms : -1;

    for(;;) {
        int r = poll(&pfd, 1, t);
        if(r > 0) {
            *revents = pfd.revents;
            return RD_SOCKET_OK;
        }

        if(r == 0) return RD_SOCKET_TIMEOUT;
        if(errno != EINTR) return RD_SOCKET_ERROR;
        // EINTR: retry (this does not shrink the remaining timeout)
    }
}

static RDSocketStatus _rd_net_socket_wait_ready(int fd, short events,
                                                u32 timeout_ms) {
    short revents = 0;
    RDSocketStatus st = _rd_net_socket_poll(fd, events, timeout_ms, &revents);
    if(st != RD_SOCKET_OK) return st;

    return ((revents & events) && !(revents & (POLLERR | POLLHUP)))
               ? RD_SOCKET_OK
               : RD_SOCKET_ERROR;
}

static void _rd_net_socket_set_send_timeout(int fd, u32 timeout_ms) {
    struct timeval tv = {
        .tv_sec = timeout_ms / 1000,
        .tv_usec = (useconds_t)((timeout_ms % 1000) * 1000),
    };

    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

static RDSocketStatus _rd_net_socket_last_error(void) {
    return errno == EAGAIN ? RD_SOCKET_TIMEOUT : RD_SOCKET_ERROR;
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
            if(_rd_net_socket_wait_ready(fd, POLLOUT, timeout_ms) ==
               RD_SOCKET_OK) {
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

    return _rd_net_socket_new(fd, kind, false);
}

RDSocket* _rd_net_socket_listen(RDSocketKind kind, const char* host, u16 port) {
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

    for(struct addrinfo* it = res; it; it = it->ai_next) {
        fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if(fd < 0) continue;

        if(kind == RD_SOCKET_TCP) {
            // allow rebinding while old connections sit in TIME_WAIT
            int on = 1;
            setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
        }

        if(bind(fd, it->ai_addr, it->ai_addrlen) == 0 &&
           (kind != RD_SOCKET_TCP || listen(fd, SOMAXCONN) == 0))
            break;

        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);

    if(fd < 0) {
        RD_LOG_FAIL("socket: listen on '%s:%u' failed", host, port);
        return NULL;
    }

    // a client can vanish between poll and accept, which must not block
    if(kind == RD_SOCKET_TCP) _rd_net_socket_set_nonblocking(fd, true);
    return _rd_net_socket_new(fd, kind, true);
}

RDSocket* _rd_net_socket_accept(RDSocket* s, u32 timeout_ms) {
    if(!s || !s->is_server || s->kind != RD_SOCKET_TCP) return NULL;

    for(;;) {
        if(_rd_net_socket_wait_ready(s->fd, POLLIN, timeout_ms) != RD_SOCKET_OK)
            return NULL;

        int fd = accept(s->fd, NULL, NULL);

        if(fd >= 0) {
            // O_NONBLOCK is inherited from the listener on BSD/macOS
            _rd_net_socket_set_nonblocking(fd, false);
            return _rd_net_socket_new(fd, RD_SOCKET_TCP, false);
        }

        // client gone before accept: keep waiting (timeout restarts)
        if(errno != EINTR && errno != EAGAIN && errno != ECONNABORTED)
            return NULL;
    }

    return NULL;
}

RDSocketResult _rd_net_socket_send(RDSocket* s, const void* data, usize n,
                                   u32 timeout_ms) {
    RDSocketResult r = {.status = RD_SOCKET_ERROR, .length = 0};
    if(!s || !data || !n) return r;

    // TCP listeners have no peer, UDP servers need a datagram to reply to
    if(s->is_server && (s->kind == RD_SOCKET_TCP || !s->peer_len)) return r;

    r.status = _rd_net_socket_wait_ready(s->fd, POLLOUT, timeout_ms);
    if(r.status != RD_SOCKET_OK) return r;

    _rd_net_socket_set_send_timeout(s->fd, timeout_ms);

    ssize_t sent = s->is_server
                       ? sendto(s->fd, data, n, MSG_NOSIGNAL,
                                (const struct sockaddr*)&s->peer, s->peer_len)
                       : send(s->fd, data, n, MSG_NOSIGNAL);

    if(sent < 0) {
        r.status = _rd_net_socket_last_error();
        return r;
    }

    r.length = (usize)sent;
    return r;
}

RDSocketResult _rd_net_socket_recv(RDSocket* s, RDScratchBuffer* reply,
                                   u32 timeout_ms) {
    RDCharVect* buf = &reply->impl;

    RDSocketResult r = {.status = RD_SOCKET_ERROR, .length = 0};
    if(!s || vect_is_empty(buf)) return r;
    if(s->is_server && s->kind == RD_SOCKET_TCP) return r;

    // no error check here: recv reports closed peers after any pending data
    short revents = 0;
    r.status = _rd_net_socket_poll(s->fd, POLLIN, timeout_ms, &revents);
    if(r.status != RD_SOCKET_OK) return r;

    // MSG_DONTWAIT: poll can report a datagram that is then dropped
    if(s->is_server) {
        struct sockaddr_storage peer;
        socklen_t peer_len = sizeof(peer);

        ssize_t n = recvfrom(s->fd, buf->data, vect_length(buf), MSG_DONTWAIT,
                             (struct sockaddr*)&peer, &peer_len);
        if(n < 0) {
            r.status = _rd_net_socket_last_error();
            return r;
        }

        // n == 0 is a valid empty datagram, not a closed connection
        s->peer = peer;
        s->peer_len = peer_len;
        r.length = (usize)n;
        return r;
    }

    ssize_t got = recv(s->fd, buf->data, vect_length(buf), MSG_DONTWAIT);

    if(got > 0)
        r.length = (usize)got;
    else if(got == 0)
        r.status = RD_SOCKET_ERROR; // peer closed the connection
    else
        r.status = _rd_net_socket_last_error();

    return r;
}

void _rd_net_socket_close(RDSocket* s) {
    if(!s) return;
    close(s->fd);
    rd_free(s);
}
