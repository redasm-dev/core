#pragma once

#include "support/utils.h"
#include <redasm/config.h>
#include <redasm/types/def.h>
#include <redasm/types/type.h>

typedef struct RDTypeFull {
    RDType base;
    RDConfidence confidence;
} RDTypeFull;

typedef struct RDResolveResultVect {
    RDResolveResult* data;
    usize length;
    usize capacity;
} RDResolveResultVect;

const char* rd_i_type_to_str(const RDType* t, RDCharVect* buf);
const char* rd_i_type_path(RDContext* ctx, RDAddress address, RDCharVect* buf);
void rd_i_type_unroll(RDContext* ctx, RDAddress address, const RDType* t);
bool rd_i_type_resolve_chain(RDContext* ctx, const RDType* root, usize offset,
                             RDResolveResultVect* out);
const RDTypeDef* rd_i_type_check_struct(const RDType* t);
bool rd_i_type_has_more(const RDType* t);
usize rd_i_size_of(const RDContext* ctx, const char* name, usize n,
                   RDTypeModifier mod);
bool rd_i_get_type(RDContext* ctx, RDAddress address, RDTypeFull* t);

bool rd_i_set_type(RDContext* ctx, RDAddress address, const char* name, usize n,
                   RDTypeModifier mod, RDConfidence c);
