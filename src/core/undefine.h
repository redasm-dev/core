#pragma once

#include <redasm/config.h>
#include <redasm/context.h>

// rewind: decoded again, keep FLOW/JMPDST/FUNC and the function record
#define RD_UNDEFINE_CLEAR ((RDUndefineFlags)(1u << 31))

bool rd_i_undefine(RDContext* self, RDAddress address, RDConfidence c,
                   RDUndefineFlags flags);
bool rd_i_undefine_n(RDContext* self, RDAddress address, usize n,
                     RDConfidence c, RDUndefineFlags flags);
bool rd_i_undefine_range(RDContext* self, RDAddress address, usize n,
                         RDConfidence c, RDUndefineFlags flags,
                         RDAddress* start, RDAddress* end);
