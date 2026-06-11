#pragma once

#include "support/utils.h"
#include <redasm/config.h>
#include <redasm/types/type.h>

typedef struct RDTypeFull {
    RDType base;
    RDConfidence confidence;
} RDTypeFull;

const char* rd_i_type_to_str(const RDType* t, RDCharVect* buf);

usize rd_i_size_of(const RDContext* ctx, const char* name, usize n,
                   RDTypeModifier flags);
bool rd_i_get_type(RDContext* ctx, RDAddress address, RDTypeFull* t);

bool rd_i_set_type(RDContext* ctx, RDAddress address, const char* name, usize n,
                   RDTypeModifier flags, RDConfidence c);
