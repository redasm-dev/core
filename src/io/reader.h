#pragma once

#include "core/segment.h"
#include "io/buffer.h"
#include <redasm/common.h>
#include <redasm/io/reader.h>

typedef struct RDReader {
    const RDSegmentFull* segment;
    RDBuffer* buffer;
    usize position;
    bool error;
    void (*seek)(RDReader* self, u64 pos);
    u64 (*get_pos)(const RDReader* self);
} RDReader;

RDReader* rd_i_reader_create(RDBuffer* buf);
RDReader* rd_i_reader_create_flags(RDContext* ctx);
void rd_i_reader_destroy(RDReader* self);
