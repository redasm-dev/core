#pragma once

#include <redasm/common.h>
#include <redasm/mapping.h>

typedef struct RDInputMapping {
    const RDContext* context;
    RDOffset offset;
    RDRelAddress rel_start;
    RDRelAddress rel_end;
} RDInputMapping;

RDInputMapping* rd_i_inputmapping_create(const RDContext* ctx, RDOffset offset,
                                         RDRelAddress start, RDRelAddress end);
void rd_i_inputmapping_destroy(RDInputMapping* self);

int rd_i_inputmapping_cmp_pred(const void* a, const void* b);
int rd_i_inputmapping_kcmp_pred(const void* key, const void* item);
