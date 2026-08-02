#pragma once

#include <redasm/common.h>
#include <redasm/segment.h>

// flags overlay
enum {
    OFL_QUEUED = (1 << 0),
};

typedef struct RDSegmentFull {
    RDSegment base;
    RDFlagsBuffer* flags;
    u32* ovl_flags;
} RDSegmentFull;

usize rd_i_address2index(const RDSegmentFull* seg, RDAddress addr);
RDAddress rd_i_index2address(const RDSegmentFull* seg, usize idx);

RDSegmentFull* rd_i_segment_create(RDContext* ctx, const char* name,
                                   RDAddress addr, RDAddress endaddr, u32 perm);

void rd_i_segment_destroy(RDSegmentFull* self);

// V Flags management
void rd_i_segment_set_queued(const RDSegmentFull* self, usize idx);
bool rd_i_segment_has_queued(const RDSegmentFull* self, usize idx);
void rd_i_segment_clear_queued(const RDSegmentFull* self, usize idx);
