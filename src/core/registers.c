#include "registers.h"
#include "core/context.h"
#include "core/db/db.h"

bool rd_i_set_regval(RDContext* ctx, RDAddress address, int reg, u64 value,
                     RDConfidence c) {
    if(!rd_i_find_segment(ctx, address)) return false;

    RDRegister oldr;
    if(rd_i_get_regval(ctx, address, reg, &oldr) && c < oldr.confidence)
        return false;

    rd_i_db_set_regval(ctx, address, reg, value, c);
    return true;
}

bool rd_i_get_regval(RDContext* ctx, RDAddress address, int reg,
                     RDRegister* r) {
    if(!rd_i_find_segment(ctx, address)) return false;
    return rd_i_db_get_regval(ctx, address, reg, r);
}

bool rd_auto_regval(RDContext* ctx, RDAddress address, int reg, u64 value) {
    return rd_i_set_regval(ctx, address, reg, value, RD_CONFIDENCE_AUTO);
}

bool rd_library_regval(RDContext* ctx, RDAddress address, int reg, u64 value) {
    return rd_i_set_regval(ctx, address, reg, value, RD_CONFIDENCE_LIBRARY);
}

bool rd_user_regval(RDContext* ctx, RDAddress address, int reg, u64 value) {
    return rd_i_set_regval(ctx, address, reg, value, RD_CONFIDENCE_USER);
}

bool rd_get_regval(RDContext* ctx, RDAddress address, int reg, u64* val) {
    if(!rd_i_find_segment(ctx, address)) return false;

    RDRegister r;

    if(rd_i_db_get_regval(ctx, address, reg, &r)) {
        if(val) *val = r.value;
    }
    else if(val)
        *val = RD_REG_UNKNOWN;

    return true;
}
