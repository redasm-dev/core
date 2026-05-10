#include "segment.h"
#include <assert.h>
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
