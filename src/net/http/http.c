#include <redasm/common.h>
#include <redasm/net/http.h>

#if defined(RD_HAS_NETWORK)
#include "net/http/http.h"
#include <redasm/net/net.h>
#endif

RDNetStatus rd_net_request(const RDNetRequest* req, RDScratchBuffer* reply,
                           u32 timeout_ms) {
#if defined(RD_HAS_NETWORK)
    if(!rd_net_is_enabled()) return (RDNetStatus){.ok = false, .code = 0};
    return _rd_net_http_request(req, reply, timeout_ms);
#else
    RD_UNUSED(req);
    RD_UNUSED(reply);
    RD_UNUSED(timeout_ms);
    return (RDNetStatus){.ok = false, .code = 0};
#endif
}

RDNetStatus rd_net_get(const char* url, RDScratchBuffer* reply,
                       u32 timeout_ms) {
    RDNetRequest req = {.method = "GET", .url = url};
    return rd_net_request(&req, reply, timeout_ms);
}

RDNetStatus rd_net_post(const char* url, const void* body, usize body_size,
                        RDScratchBuffer* reply, u32 timeout_ms) {
    RDNetRequest req = {
        .method = "POST",
        .url = url,
        .body = body,
        .body_size = body_size,
    };

    return rd_net_request(&req, reply, timeout_ms);
}

RDNetStatus rd_net_put(const char* url, const void* body, usize body_size,
                       RDScratchBuffer* reply, u32 timeout_ms) {
    RDNetRequest req = {
        .method = "PUT",
        .url = url,
        .body = body,
        .body_size = body_size,
    };

    return rd_net_request(&req, reply, timeout_ms);
}

RDNetStatus rd_net_delete(const char* url, RDScratchBuffer* reply,
                          u32 timeout_ms) {
    RDNetRequest req = {.method = "DELETE", .url = url};
    return rd_net_request(&req, reply, timeout_ms);
}
