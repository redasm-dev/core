#pragma once

#include <redasm/config.h>
#include <redasm/support/scratch.h>

typedef struct RDNetHeader {
    const char* key;
    const char* value;
} RDNetHeader;

typedef struct RDNetRequest {
    const char* method;
    const char* url;
    const char* user_agent;
    const RDNetHeader* headers; // NULL terminated
    const void* body;
    usize body_size;
} RDNetRequest;

typedef struct RDNetStatus {
    bool ok;
    int code;
} RDNetStatus;

static inline bool rd_net_http_status_ok(RDNetStatus st) {
    return st.ok && st.code == 200;
}

// clang-format off
RD_API RDNetStatus rd_net_http_request(const RDNetRequest* req, RDScratchBuffer* reply, u32 timeout_ms);
RD_API RDNetStatus rd_net_http_get(const char* url, RDScratchBuffer* reply, u32 timeout_ms);
RD_API RDNetStatus rd_net_http_post(const char* url, const void* body, usize body_n, RDScratchBuffer* reply, u32 timeout_ms);
RD_API RDNetStatus rd_net_http_put(const char* url, const void* body, usize body_n, RDScratchBuffer* reply, u32 timeout_ms);
RD_API RDNetStatus rd_net_http_delete(const char* url, RDScratchBuffer* reply, u32 timeout_ms);
// clang-format on
