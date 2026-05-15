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

    struct {
        usize* data;
        usize length;
        usize capacity;
    } stack;

    void (*seek)(RDReader* self, u64 pos);
    u64 (*tell)(const RDReader* self);
} RDReader;

RDReader* rd_i_reader_create(RDBuffer* buf);
RDReader* rd_i_reader_create_flags(RDContext* ctx);
void rd_i_reader_destroy(RDReader* self);
