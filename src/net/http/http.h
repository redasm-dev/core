#pragma once

#include <redasm/net/http.h>
#include <redasm/support/scratch.h>

#define RD_NET_USERAGENT "REDasm/" RD_VERSION

typedef struct RDNetHttp RDNetHttp;

void _rd_net_http_init(void);
void _rd_net_http_deinit(void);

RDNetStatus _rd_net_http_request(const RDNetRequest* req,
                                 RDScratchBuffer* reply, u32 timeout_ms);
