#pragma once

#include <redasm/common.h>
#include <redasm/config.h>

#define RD_REG_UNKNOWN UINT64_MAX

typedef struct RDTrackedRegister {
    RDAddress address;
    int reg;
    u64 value;
} RDTrackedRegister;

typedef struct RDTrackedRegisterSlice {
    const RDTrackedRegister* data;
    usize length;
} RDTrackedRegisterSlice;

// clang-format off
RD_API bool rd_auto_regval(RDContext* ctx, RDAddress address, int reg, u64 value);
RD_API bool rd_library_regval(RDContext* ctx, RDAddress address, int reg, u64 value);
RD_API bool rd_user_regval(RDContext* ctx, RDAddress address, int reg, u64 value);
RD_API bool rd_get_regval(RDContext* ctx, RDAddress address, int reg, u64* value);
RD_API RDTrackedRegisterSlice rd_get_all_registers(RDContext* ctx);
// clang-format on
