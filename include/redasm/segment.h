#pragma once

#include <redasm/config.h>
#include <redasm/io/buffer.h>

typedef struct RDSegment {
    const char* name;
    RDAddress start_address;
    RDAddress end_address;
    u32 perm;
    u32 unit;
} RDSegment;

typedef struct RDSegmentSlice {
    const RDSegment** data;
    usize length;
} RDSegmentSlice;

RD_API const RDFlagsBuffer* rd_segment_get_flags(const RDSegment* self);
