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

typedef struct RDSegmentReg {
    RDAddress address;
    const char* name;
    RDRegValue value;
    bool has_value;
    RDConfidence confidence;
} RDSegmentReg;

typedef struct RDSegmentRegSlice {
    const RDSegmentReg* data;
    usize length;
} RDSegmentRegSlice;

typedef struct RDSegmentRegNameSlice {
    const char** data;
    usize length;
} RDSegmentRegNameSlice;

// clang-format off
RD_API bool rd_set_regval(RDContext* ctx, const char* regname, RDRegValue value);
RD_API bool rd_get_regval(RDContext* ctx, const char* regname, RDRegValue* value);
RD_API bool rd_del_regval(RDContext* ctx, const char* regname);

RD_API bool rd_set_regval_id(RDContext* ctx, RDReg id, RDRegValue value);
RD_API bool rd_get_regval_id(RDContext* ctx, RDReg id, RDRegValue* value);
RD_API bool rd_del_regval_id(RDContext* ctx, RDReg id);

RD_API bool rd_auto_sregval(RDContext* ctx, RDAddress address, const char* regname, RDRegValue value);
RD_API bool rd_library_sregval(RDContext* ctx, RDAddress address, const char* regname, RDRegValue value);
RD_API bool rd_user_sregval(RDContext* ctx, RDAddress address, const char* regname, RDRegValue value);
RD_API bool rd_del_auto_sregval(RDContext* ctx, RDAddress address, const char* regname);
RD_API bool rd_del_library_sregval(RDContext* ctx, RDAddress address, const char* regname);
RD_API bool rd_del_user_sregval(RDContext* ctx, RDAddress address, const char* regname);
RD_API bool rd_get_sregval(RDContext* ctx, RDAddress address, const char* regname, RDRegValue* value);

RD_API bool rd_auto_sregval_id(RDContext* ctx, RDAddress address, RDReg id, RDRegValue value);
RD_API bool rd_library_sregval_id(RDContext* ctx, RDAddress address, RDReg id, RDRegValue value);
RD_API bool rd_user_sregval_id(RDContext* ctx, RDAddress address, RDReg id, RDRegValue value);
RD_API bool rd_del_auto_sregval_id(RDContext* ctx, RDAddress address, RDReg id);
RD_API bool rd_del_library_sregval_id(RDContext* ctx, RDAddress address, RDReg id);
RD_API bool rd_del_user_sregval_id(RDContext* ctx, RDAddress address, RDReg id);
RD_API bool rd_get_sregval_id(RDContext* ctx, RDAddress address, RDReg id, RDRegValue* value);

RD_API RDSegmentRegNameSlice rd_get_all_sreg_names(const RDContext* ctx);
RD_API RDSegmentRegSlice rd_get_all_sregval(RDContext* ctx, const char* regname);
// clang-format on
