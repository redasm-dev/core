#pragma once

#include "db/types.h"
#include <redasm/config.h>

#define RD_UNDEFINE_PUBLIC_MASK 0xFFFF

// rewind: decoded again, keep FLOW/JMPDST/FUNC and the function record
#define RD_UNDEFINE_CLEAR ((RDUndefineFlags)(1u << 31))

typedef struct RDUndefineState {
    RDXRefVect xrefs;
    RDAddress start;
    RDAddress end;
    RDConfidence confidence;
    RDUndefineFlags flags;
} RDUndefineState;

bool rd_i_undefine(RDContext* self, RDAddress address, RDConfidence c,
                   RDUndefineFlags flags);
bool rd_i_undefine_n(RDContext* self, RDAddress address, usize n,
                     RDConfidence c, RDUndefineFlags flags);
bool rd_i_undefine_range(RDContext* self, RDAddress address, usize n,
                         RDConfidence c, RDUndefineFlags flags,
                         RDAddress* start, RDAddress* end);
