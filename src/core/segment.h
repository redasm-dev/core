#pragma once

#include <redasm/segment.h>

typedef struct RDSegmentFull {
    RDSegment base;
    RDFlagsBuffer* flags;
} RDSegmentFull;

usize rd_i_address2index(const RDSegmentFull* self, RDAddress addr);
RDAddress rd_i_index2address(const RDSegmentFull* self, usize idx);
