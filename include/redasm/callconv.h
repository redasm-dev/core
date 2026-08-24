#pragma once

#include <redasm/config.h>

typedef enum {
    RD_ARGORDER_RTL,
    RD_ARGORDER_LTR,
} RDArgOrder;

typedef enum {
    RD_STACK_CLEANUP_CALLER,
    RD_STACK_CLEANUP_CALLEE,
} RDStackCleanup;

typedef struct RDCallConv RDCallConv;

RD_API const char* rd_callconv_name(const RDCallConv* self);
RD_API RDArgOrder rd_callconv_arg_order(const RDCallConv* self);
RD_API usize rd_callconv_shadow_space(const RDCallConv* self);
RD_API RDStackCleanup rd_callconv_stack_cleanup(const RDCallConv* self);
RD_API usize rd_callconv_n_arg_regs(const RDCallConv* self);
RD_API const char* rd_callconv_arg_reg(const RDCallConv* self, usize idx);
