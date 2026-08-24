#pragma once

#include <redasm/callconv.h>
#include <redasm/common.h>

typedef struct RDArgRegsVect {
    const char** data;
    usize length;
    usize capacity;
} RDArgRegsVect;

typedef struct RDCallConv {
    const char* name; // interned

    RDArgRegsVect arg_regs; // interned canonical register name
    RDArgOrder arg_order;
    RDStackCleanup stack_cleanup;
    usize shadow_space;
} RDCallConv;

typedef struct RDCallConvVect {
    RDCallConv** data;
    usize length;
    usize capacity;
} RDCallConvVect;

RDCallConv* rd_i_callconv_create(const char* name, RDContext* ctx);
void rd_i_callconv_destroy(RDCallConv* self);
const RDCallConv* rd_i_callconv_find(const RDContext* ctx, const char* name);
