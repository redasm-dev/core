#pragma once

#include "support/containers.h"
#include <redasm/registers.h>

typedef struct RDRegResolved {
    const char* name; // interned, canonical
    RDRegMask mask;
} RDRegResolved;

typedef struct RDRegister {
    HMapHeader hmap_hdr;

    const char* name;
    RDRegValue value;
} RDRegister;

typedef struct RDRegisterHMap {
    RDRegister* data;
    usize length;
    usize capacity;
    usize tombstones;
    HMapHash hash;
    HMapEqual equal;
} RDRegisterHMap;

void rd_i_registermap_init(RDRegisterHMap* self);

bool rd_i_reg_resolve_id(RDContext* ctx, RDReg id, RDRegResolved* out);
bool rd_i_reg_resolve_name(RDContext* ctx, const char* regname,
                           RDRegResolved* out);

bool rd_i_regmap_set(RDRegisterHMap* map, const RDRegResolved* res,
                     RDRegValue value);
bool rd_i_regmap_get(const RDRegisterHMap* map, const RDRegResolved* res,
                     RDRegValue* value);
bool rd_i_regmap_del(RDRegisterHMap* map, const RDRegResolved* res);
