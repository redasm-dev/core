#include "mapping.h"
#include "core/context.h"
#include <redasm/allocator.h>

int rd_i_inputmapping_cmp_pred(const void* a, const void* b) {
    const RDInputMapping* sa = *(const RDInputMapping**)a;
    const RDInputMapping* sb = *(const RDInputMapping**)b;
    if(sa->rel_start < sb->rel_start) return -1;
    if(sa->rel_start > sb->rel_start) return 1;
    return 0;
}

int rd_i_inputmapping_kcmp_pred(const void* key, const void* item) {
    RDRelAddress rel_addr = *(const RDRelAddress*)key;
    const RDInputMapping* m = *(const RDInputMapping**)item;
    if(rel_addr < m->rel_start) return -1;
    if(rel_addr >= m->rel_end) return 1;
    return 0;
}

RDInputMapping* rd_i_inputmapping_create(const RDContext* ctx, RDOffset offset,
                                         RDRelAddress start, RDRelAddress end) {
    RDInputMapping* self = rd_alloc(sizeof(*self));

    (*self) = (RDInputMapping){
        .context = ctx,
        .offset = offset,
        .rel_start = start,
        .rel_end = end,
    };

    return self;
}

void rd_i_inputmapping_destroy(RDInputMapping* self) { rd_free(self); }

RDOffset rd_inputmapping_get_offset(const RDInputMapping* self) {
    return self->offset;
}

RDAddress rd_inputmapping_get_start(const RDInputMapping* self) {
    return rd_i_abs(self->context, self->rel_start);
}

RDAddress rd_inputmapping_get_end(const RDInputMapping* self) {
    return rd_i_abs(self->context, self->rel_end);
}

usize rd_inputmapping_get_size(const RDInputMapping* self) {
    return self->rel_end - self->rel_start;
}
