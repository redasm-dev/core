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
    struct {
        RDType value;
        bool has_value;
    } ret;

    struct {
        RDParamVect value;
        bool has_value;
    } args;

    bool is_noret;
} RDFunctionType;

typedef enum {
    RD_TFLAGS_NONE = 0,
    RD_TFLAGS_BUILTIN = (1 << 0),
    RD_TFLAGS_STATIC = (1 << 1),
    RD_TFLAGS_SIGNED = (1 << 2), // RD_TKIND_PRIM only
} RDTypeFlags;

typedef struct RDTypeDef {
    const char* name;
    usize size;
    RDTypeKind kind;
    RDTypeFlags flags;

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

void rd_i_typedef_register_builtins(RDContext* ctx);
void rd_i_typedef_measure(const RDContext* ctx, RDTypeDef* tdef);
RDTypeDef* rd_i_typedef_find(const RDContext* ctx, const char* name);

static inline bool rd_i_typedef_is_compound(const RDTypeDef* self) {
    return self->kind == RD_TKIND_STRUCT || self->kind == RD_TKIND_UNION;
}
