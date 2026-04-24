#pragma once

#include "core/db/types.h"
#include <redasm/config.h>
#include <redasm/types/type.h>

typedef struct RDTypeFull {
    RDType base;
    RDConfidence confidence;
} RDTypeFull;

static inline bool rd_i_is_void(const RDType* t) {
    return !t->name || !(*t->name);
}

usize rd_i_size_of(const RDContext* ctx, const char* name, usize n,
                   RDTypeFlags flags);
bool rd_i_get_type(RDContext* ctx, RDAddress address, RDTypeFull* t);

bool rd_i_set_type(RDContext* ctx, RDAddress address, const char* name, usize n,
                   RDTypeFlags flags, RDConfidence c);
