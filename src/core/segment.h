#pragma once

#include <redasm/common.h>
#include <redasm/segment.h>

// flags overlay
enum {
    OFL_QUEUED = (1 << 0),
};

typedef struct RDSegment {
    const RDContext* context;
    RDFlagsBuffer* flags;
    u8* ovl_flags;
    const char* name;
    RDRelAddress rel_start;
    RDRelAddress rel_end;
    RDSegmentPerm perm;
} RDSegment;

usize rd_i_address2index(const RDSegment* seg, RDAddress addr);
RDAddress rd_i_index2address(const RDSegment* seg, usize idx);

RDSegment* rd_i_segment_create(RDContext* ctx, const char* name,
                               RDRelAddress addr, RDRelAddress endaddr,
                               u32 perm);

void rd_i_segment_destroy(RDSegment* self);

// Overlay Flags management
void rd_i_segment_set_queued(const RDSegment* self, usize idx);
bool rd_i_segment_has_queued(const RDSegment* self, usize idx);
void rd_i_segment_clear_queued(const RDSegment* self, usize idx);
