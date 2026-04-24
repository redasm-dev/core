#pragma once

#include <redasm/segment.h>

typedef struct RDSegmentFull {
    RDSegment base;
    RDFlagsBuffer* flags;
} RDSegmentFull;

usize rd_i_address2index(const RDSegmentFull* self, RDAddress addr);
RDAddress rd_i_index2address(const RDSegmentFull* self, usize idx);

int rd_i_segment_cmp_pred(const void* a, const void* b);
int rd_i_segment_find_pred(const void* key, const void* item);
