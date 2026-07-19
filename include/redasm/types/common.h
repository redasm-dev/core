#pragma once

#include <redasm/common.h>
#include <redasm/config.h>

typedef struct RDTypeDef RDTypeDef;

typedef enum {
    RD_TYPE_NONE = 0,
    RD_TYPE_PTR,
    RD_TYPE_CPTR,
} RDTypeModifier;

typedef struct RDType {
    const RDTypeDef* def;
    usize count;
    RDTypeModifier mod;
} RDType;

typedef struct RDParam {
    RDType type;
    const char* name;
    usize field_offset;
} RDParam;

typedef struct RDParamSlice {
    const RDParam* data;
    usize length;
} RDParamSlice;
