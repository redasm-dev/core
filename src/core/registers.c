#include "core/context.h"
#include "core/db/types.h"
#include <redasm/registers.h>

static bool _rd_i_set_regval(RDContext* ctx, RDAddress address, int reg,
                             u64 value, RDConfidence c) {
    if(!rd_i_find_segment(ctx, address)) return false;

    RDRegister oldr;
    if(rd_i_db_get_regval_exact(ctx, address, reg, &oldr)) {
        if(c < oldr.confidence) return false;
        if(c == oldr.confidence && value == oldr.value) return false;
    }

    rd_i_db_set_regval(ctx, address, reg, value, c);
    return true;
}

bool rd_auto_regval(RDContext* ctx, RDAddress address, int reg, u64 value) {
    return _rd_i_set_regval(ctx, address, reg, value, RD_CONFIDENCE_AUTO);
}

bool rd_library_regval(RDContext* ctx, RDAddress address, int reg, u64 value) {
    return _rd_i_set_regval(ctx, address, reg, value, RD_CONFIDENCE_LIBRARY);
}

bool rd_user_regval(RDContext* ctx, RDAddress address, int reg, u64 value) {
    return _rd_i_set_regval(ctx, address, reg, value, RD_CONFIDENCE_USER);
}

bool rd_get_regval(RDContext* ctx, RDAddress address, int reg, u64* val) {
    if(!rd_i_find_segment(ctx, address)) return false;

    RDRegister r;

    if(rd_i_db_get_regval(ctx, address, reg, &r)) {
        if(val) *val = r.value;
        return true;
    }

    return false;
}
