#include "net/http/http.h"
#include <redasm/common.h>

void _rd_net_http_init(void) {}
void _rd_net_http_deinit(void) {}

RDNetStatus _rd_net_http_request(const RDNetRequest* req,
                                 RDScratchBuffer* reply, u32 timeout_ms) {
    RD_UNUSED(req);
    RD_UNUSED(reply);
    RD_UNUSED(timeout_ms);
    return (RDNetStatus){.ok = false, .code = 0};
}
