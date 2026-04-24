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

int rd_i_segment_cmp_pred(const void* a, const void* b) {
    const RDSegmentFull* sa = *(const RDSegmentFull**)a;
    const RDSegmentFull* sb = *(const RDSegmentFull**)b;
    if(sa->base.start_address < sb->base.start_address) return -1;
    if(sa->base.start_address > sb->base.start_address) return 1;
    return 0;
}

int rd_i_segment_find_pred(const void* key, const void* item) {
    RDAddress addr = *(const RDAddress*)key;
    const RDSegmentFull* seg = *(const RDSegmentFull**)item;
    if(addr < seg->base.start_address) return -1;
    if(addr >= seg->base.end_address) return 1;
    return 0;
}
