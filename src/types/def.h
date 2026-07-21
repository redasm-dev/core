#pragma once

#include <redasm/types/def.h>

typedef struct RDParamVect {
    RDParam* data;
    usize length;
    usize capacity;
} RDParamVect;

typedef struct RDEnumCase {
    const char* name;
    i64 value;
} RDEnumCase;

typedef struct RDEnumType {
    const RDTypeDef* base_type;
    RDEnumCase* data;
    usize length;
    usize capacity;
} RDEnumType;

typedef struct RDFunctionType {
    RDType ret;
    RDParamVect args;
    bool is_noret;
} RDFunctionType;

typedef struct RDTypeDef {
    const char* name;
    usize size;
    RDTypeKind kind;

    union {
        RDParamVect compound_;
        RDEnumType enum_;
        RDFunctionType func_;
    };
} RDTypeDef;

typedef struct RDTypeDefVect {
    RDTypeDef** data;
    usize length;
    usize capacity;
} RDTypeDefVect;

void rd_i_register_primitives(RDContext* ctx);
void rd_i_typedef_measure(const RDContext* ctx, RDTypeDef* tdef);
RDTypeDef* rd_i_typedef_find(const RDContext* ctx, const char* name);

static inline bool rd_i_typedef_is_compound(const RDTypeDef* self) {
    return self->kind == RD_TKIND_STRUCT || self->kind == RD_TKIND_UNION;
}
