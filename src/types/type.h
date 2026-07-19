#pragma once

#include "support/utils.h"
#include <redasm/config.h>
#include <redasm/types/def.h>
#include <redasm/types/type.h>

typedef struct RDTypeFull {
    RDType base;
    RDConfidence confidence;
} RDTypeFull;

const char* rd_i_type_to_str(const RDType* t, RDCharVect* buf);
void rd_i_type_unroll(RDContext* ctx, RDAddress address, const RDType* t);
const RDTypeDef* rd_i_type_check_struct(const RDType* t);
bool rd_i_type_has_more(const RDType* t);

usize rd_i_size_of(const RDContext* ctx, const char* name, usize n,
                   RDTypeModifier mod);
bool rd_i_get_type(RDContext* ctx, RDAddress address, RDTypeFull* t);

bool rd_i_set_type(RDContext* ctx, RDAddress address, const char* name, usize n,
                   RDTypeModifier mod, RDConfidence c);
