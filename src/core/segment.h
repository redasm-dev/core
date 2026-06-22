#pragma once

#include <redasm/common.h>
#include <redasm/segment.h>

typedef struct RDSegmentFull {
    RDSegment base;
    RDFlagsBuffer* flags;
} RDSegmentFull;

usize rd_i_address2index(const RDSegmentFull* self, RDAddress addr);
RDAddress rd_i_index2address(const RDSegmentFull* self, usize idx);

RDSegmentFull* rd_i_segment_create(RDContext* ctx, const char* name,
                                   RDAddress addr, RDAddress endaddr, u32 perm,
                                   u32 unit);
void rd_i_segment_destroy(RDSegmentFull* self);
