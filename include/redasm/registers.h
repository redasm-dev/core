#pragma once

#include <redasm/common.h>
#include <redasm/config.h>

#define RD_REG_UNKNOWN UINT64_MAX

// clang-format off
RD_API bool rd_auto_regval(RDContext* ctx, RDAddress address, int reg, u64 value);
RD_API bool rd_library_regval(RDContext* ctx, RDAddress address, int reg, u64 value);
RD_API bool rd_user_regval(RDContext* ctx, RDAddress address, int reg, u64 value);
RD_API bool rd_get_regval(RDContext* ctx, RDAddress address, int reg, u64* value);
// clang-format on
