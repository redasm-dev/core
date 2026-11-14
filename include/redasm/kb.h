#pragma once

#include <redasm/common.h>
#include <redasm/support/datum.h>

RD_API const RDDatum* rd_kb_load(RDContext* ctx, const char* kb);
RD_API RDStringSlice rd_kb_get_paths(void);
