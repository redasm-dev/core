#pragma once

#include <redasm/config.h>
#include <redasm/io/buffer.h>

typedef struct RDSegment RDSegment;

typedef enum {
    RD_SP_NONE = 0,
    RD_SP_R = 1 << 0,
    RD_SP_W = 1 << 1,
    RD_SP_X = 1 << 2,

    RD_SP_RW = RD_SP_R | RD_SP_W,
    RD_SP_RX = RD_SP_R | RD_SP_X,
    RD_SP_RWX = RD_SP_R | RD_SP_W | RD_SP_X,
    RD_SP_WX = RD_SP_W | RD_SP_X,
} RDSegmentPerm;

typedef struct RDSegmentSlice {
    const RDSegment** data;
    usize length;
} RDSegmentSlice;

RD_API const RDFlagsBuffer* rd_segment_get_flags(const RDSegment* self);
RD_API RDSegmentPerm rd_segment_get_perm(const RDSegment* self);
RD_API bool rd_segment_has_perm(const RDSegment* self, RDSegmentPerm p);
RD_API const char* rd_segment_get_name(const RDSegment* self);
RD_API RDAddress rd_segment_get_start(const RDSegment* self);
RD_API RDAddress rd_segment_get_end(const RDSegment* self);
RD_API usize rd_segment_get_size(const RDSegment* self);
