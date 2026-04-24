#pragma once

#include <redasm/types/def.h>

typedef enum {
    RD_TKIND_PRIM = 0,
    RD_TKIND_STRUCT,
    RD_TKIND_UNION,
    RD_TKIND_ENUM,
    RD_TKIND_FUNC,
} RDTypeKind;

typedef struct RDParam {
    RDType type;
    const char* name;
} RDParam;

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
    const char* base_type;
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

RDTypeDef* rd_i_typedef_find(const RDContext* ctx, const char* name,
                             bool required);
void rd_i_typedef_destroy(RDTypeDef* self);
void rd_i_typedef_resolve_size(const RDContext* ctx, RDTypeDef* tdef);
void rd_i_register_primitives(RDContext* ctx);

static inline bool rd_i_typedef_is_compound(const RDTypeDef* self) {
    return self->kind == RD_TKIND_STRUCT || self->kind == RD_TKIND_UNION;
}
