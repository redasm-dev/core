#pragma once

#include <redasm/common.h>
#include <redasm/config.h>

#define RD_REGMASK_FULL UINT64_MAX
#define RD_REGID_UNKNOWN UINT32_MAX

typedef u32 RDReg;
typedef u64 RDRegValue;

typedef struct RDRegMask {
    RDReg reg;
    RDRegValue mask;
    u8 shift;
} RDRegMask;

typedef struct RDTrackedReg {
    RDAddress address;

    struct {
        const char* name;
        RDRegValue value;
        bool has_value;
    } reg;
} RDTrackedReg;

typedef struct RDTrackedRegSlice {
    const RDTrackedReg* data;
    usize length;
} RDTrackedRegSlice;

// clang-format off
RD_API bool rd_auto_regval(RDContext* ctx, RDAddress address, const char* regname, RDRegValue value);
RD_API bool rd_library_regval(RDContext* ctx, RDAddress address, const char* regname, RDRegValue value);
RD_API bool rd_user_regval(RDContext* ctx, RDAddress address, const char* regname, RDRegValue value);
RD_API bool rd_get_regval(RDContext* ctx, RDAddress address, const char* regname, RDRegValue* value);
RD_API bool rd_del_auto_regval(RDContext* ctx, RDAddress address, const char* regname);
RD_API bool rd_del_library_regval(RDContext* ctx, RDAddress address, const char* regname);
RD_API bool rd_del_user_regval(RDContext* ctx, RDAddress address, const char* regname);

RD_API bool rd_auto_regval_id(RDContext* ctx, RDAddress address, RDReg reg, RDRegValue value);
RD_API bool rd_library_regval_id(RDContext* ctx, RDAddress address, RDReg reg, RDRegValue value);
RD_API bool rd_user_regval_id(RDContext* ctx, RDAddress address, RDReg reg, RDRegValue value);
RD_API bool rd_get_regval_id(RDContext* ctx, RDAddress address, RDReg id, RDRegValue* value);
RD_API bool rd_del_auto_regval_id(RDContext* ctx, RDAddress address, RDReg id);
RD_API bool rd_del_library_regval_id(RDContext* ctx, RDAddress address, RDReg id);
RD_API bool rd_del_user_regval_id(RDContext* ctx, RDAddress address, RDReg id);

RD_API RDTrackedRegSlice rd_get_all_reg(RDContext* ctx);
// clang-format on
