#pragma once

#include <redasm/registers.h>

typedef struct RDRegResolved {
    const char* name; // interned, root
    RDRegMask mask;
} RDRegResolved;

typedef struct RDRegister {
    const char* name;
    RDRegValue value;
    RDRegValue known_bits;
    RDRegSetter setter;
    RDRegValue setter_bits;
} RDRegister;

typedef struct RDRegisterVect {
    RDRegister* data;
    usize length;
    usize capacity;
} RDRegisterVect;

static inline RDRegValue rd_i_get_regmask(u8 bits, u8 shift) {
    if(!bits || (unsigned int)bits + shift > 64) return 0;

    RDRegValue ones = bits == 64 ? ~(RDRegValue)0
                                 : ((RDRegValue)1 << bits) - 1; // 1 << 64: UB
    return ones << shift;
}

bool rd_i_reg_resolve_id(RDContext* ctx, RDReg id, RDRegResolved* out);
bool rd_i_reg_resolve_name(RDContext* ctx, const char* regname,
                           RDRegResolved* out);

bool rd_i_regmap_set(RDRegisterVect* self, const RDRegResolved* res,
                     RDRegValue value, RDRegSetter setter);
bool rd_i_regmap_get(const RDRegisterVect* self, const RDRegResolved* res,
                     RDRegValue* value);
bool rd_i_regmap_del(RDRegisterVect* self, const RDRegResolved* res);

bool rd_i_regmap_get_info(const RDRegisterVect* self, const RDRegResolved* res,
                          RDRegValueInfo* info);
