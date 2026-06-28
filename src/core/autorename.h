#pragma once

#include "core/context.h"

const char* rd_i_resolve_name(RDContext* ctx, RDIL* rdil, RDAddress* resolved,
                              RDConfidence* c);
void rd_i_autorename(RDContext* ctx);
