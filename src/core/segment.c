#include "segment.h"
#include "core/context.h"
#include "io/buffer.h"
#include "io/flagsbuffer.h"
#include "support/logging.h"
#include "support/stringpool.h"
#include <assert.h>
#include <redasm/allocator.h>
#include <redasm/segment.h>

usize rd_i_address2index(const RDSegmentFull* self, RDAddress addr) {
    assert(addr >= self->base.start_address && addr < self->base.end_address &&
           "address out of range");
    return addr - self->base.start_address;
}

RDAddress rd_i_index2address(const RDSegmentFull* self, usize idx) {
    RDAddress addr = self->base.start_address + idx;
    assert(addr >= self->base.start_address && addr < self->base.end_address &&
           "index out of range");
    return addr;
}

const RDFlagsBuffer* rd_segment_get_flags(const RDSegment* self) {
    return ((const RDSegmentFull*)self)->flags;
}

RDSegmentFull* rd_i_segment_create(RDContext* ctx, const char* name,
                                   RDAddress addr, RDAddress endaddr, u32 perm,
                                   u32 unit) {
    assert(name);

    if(addr >= endaddr) {
        LOG_FAIL("invalid address range for segment '%s'", name);
        return NULL;
    }

    RDSegmentFull* s = rd_alloc(sizeof(*s));

    *s = (RDSegmentFull){
        .base =
            {
                .name = rd_i_strpool_intern(&ctx->strings, name),
                .start_address = addr,
                .end_address = endaddr,
                .perm = perm,
                .unit = unit,
            },

        .flags = rd_i_flagsbuffer_create(endaddr - addr),
    };

    return s;
}

void rd_i_segment_destroy(RDSegmentFull* self) {
    rd_i_buffer_destroy((RDBuffer*)self->flags);
    rd_free(self);
}
