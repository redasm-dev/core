#pragma once

#include "core/db/types.h"
#include <redasm/registers.h>

bool rd_i_set_regval(RDContext* ctx, RDAddress addr, int reg, u64 value,
                     RDConfidence c);

bool rd_i_get_regval(RDContext* ctx, RDAddress addr, int reg, RDRegister* r);
