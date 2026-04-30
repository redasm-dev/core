#pragma once

#include <redasm/common.h>
#include <redasm/config.h>

#define RD_REGVAL_UNKNOWN UINT64_MAX
#define RD_REGID_UNKNOWN UINT32_MAX

typedef u32 RDReg;
typedef u64 RDRegValue;

typedef struct RDTrackedReg {
    RDAddress address;
    RDReg reg;
    RDRegValue value;
} RDTrackedReg;

typedef struct RDTrackedRegisterSlice {
    const RDTrackedReg* data;
    usize length;
} RDTrackedRegSlice;

// clang-format off
RD_API bool rd_auto_regval(RDContext* ctx, RDAddress address, RDReg reg, RDRegValue value);
RD_API bool rd_library_regval(RDContext* ctx, RDAddress address, RDReg reg, RDRegValue value);
RD_API bool rd_user_regval(RDContext* ctx, RDAddress address, RDReg reg, RDRegValue value);
RD_API bool rd_get_regval(RDContext* ctx, RDAddress address, RDReg reg, RDRegValue* value);
RD_API RDTrackedRegSlice rd_get_all_reg(RDContext* ctx);
// clang-format on
