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

// clang-format off
RD_API RDNetStatus rd_net_request(const RDNetRequest* req, RDScratchBuffer* reply, u32 timeout_ms);
RD_API RDNetStatus rd_net_get(const char* url, RDScratchBuffer* reply, u32 timeout_ms);
RD_API RDNetStatus rd_net_post(const char* url, const void* body, usize body_n, RDScratchBuffer* reply, u32 timeout_ms);
RD_API RDNetStatus rd_net_put(const char* url, const void* body, usize body_n, RDScratchBuffer* reply, u32 timeout_ms);
RD_API RDNetStatus rd_net_delete(const char* url, RDScratchBuffer* reply, u32 timeout_ms);
// clang-format on
