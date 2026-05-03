#include "core/context.h"
#include "core/db/types.h"
#include "support/containers.h"
#include <redasm/registers.h>

static bool _rd_reg_getmask(RDContext* ctx, const char* regname, RDRegMask* m) {
    if(ctx->processorplugin->get_reg_mask) {
        if(!ctx->processorplugin->get_reg_name) return false;

        if(!ctx->processorplugin->get_reg_mask(regname, m, ctx->processor))
            return false;

        return ctx->processorplugin->get_reg_name(m->reg, ctx->processor);
    }

    return false;
}

static bool _rd_set_regval(RDContext* ctx, RDAddress address,
                           const char* regname, u64 value, RDConfidence c) {
    if(!regname || !rd_i_find_segment(ctx, address)) return false;

    RDRegMask m = {.mask = RD_REGMASK_FULL, .shift = 0};

    if(_rd_reg_getmask(ctx, regname, &m))
        regname = ctx->processorplugin->get_reg_name(m.reg, ctx->processor);

    if(!regname) return false;

    // merge if bit field
    if(m.mask != RD_REGMASK_FULL) {
        RDRegEntry curr;
        RDRegValue base = 0;

        if(rd_i_db_get_regval(ctx, address, regname, &curr) &&
           curr.reg.has_value) {
            base = curr.reg.value;
        }

        value = (base & ~m.mask) | ((value << m.shift) & m.mask);
    }

    RDRegEntry e;
    if(rd_i_db_get_regval_exact(ctx, address, regname, &e)) {
        if(c < e.confidence) return false;

        if(c == e.confidence && e.reg.has_value && e.reg.value == value)
            return false;
    }

    rd_i_db_set_regval(ctx, address, regname, value, c);
    return true;
}

static bool _rd_del_regval(RDContext* ctx, RDAddress address,
                           const char* regname, RDConfidence c) {
    if(!regname || !rd_i_find_segment(ctx, address)) return false;

    RDRegMask m = {.mask = RD_REGMASK_FULL, .shift = 0};

    if(_rd_reg_getmask(ctx, regname, &m))
        regname = ctx->processorplugin->get_reg_name(m.reg, ctx->processor);

    if(!regname) return false;

    RDRegEntry e;
    if(rd_i_db_get_regval_exact(ctx, address, regname, &e)) {
        if(c < e.confidence) return false;
        if(c == e.confidence && !e.reg.has_value) return false; // already NULL
    }

    rd_i_db_del_regval(ctx, address, regname, c);
    return true;
}

bool rd_auto_regval(RDContext* ctx, RDAddress address, const char* regname,
                    RDRegValue value) {
    return _rd_set_regval(ctx, address, regname, value, RD_CONFIDENCE_AUTO);
}

bool rd_library_regval(RDContext* ctx, RDAddress address, const char* regname,
                       RDRegValue value) {
    return _rd_set_regval(ctx, address, regname, value, RD_CONFIDENCE_LIBRARY);
}

bool rd_user_regval(RDContext* ctx, RDAddress address, const char* regname,
                    RDRegValue value) {
    return _rd_set_regval(ctx, address, regname, value, RD_CONFIDENCE_USER);
}

bool rd_auto_regval_id(RDContext* ctx, RDAddress address, RDReg reg,
                       RDRegValue value) {
    if(!ctx->processorplugin->get_reg_name) return false;

    const char* regname =
        ctx->processorplugin->get_reg_name(reg, ctx->processor);
    return rd_auto_regval(ctx, address, regname, value);
}

bool rd_library_regval_id(RDContext* ctx, RDAddress address, RDReg reg,
                          RDRegValue value) {
    if(!ctx->processorplugin->get_reg_name) return false;

    const char* regname =
        ctx->processorplugin->get_reg_name(reg, ctx->processor);
    return rd_library_regval(ctx, address, regname, value);
}

bool rd_user_regval_id(RDContext* ctx, RDAddress address, RDReg reg,
                       RDRegValue value) {
    if(!ctx->processorplugin->get_reg_name) return false;

    const char* regname =
        ctx->processorplugin->get_reg_name(reg, ctx->processor);
    return rd_user_regval(ctx, address, regname, value);
}

bool rd_get_regval(RDContext* ctx, RDAddress address, const char* regname,
                   RDRegValue* value) {
    if(!rd_i_find_segment(ctx, address) || !regname) return false;

    RDRegMask m = {.mask = RD_REGMASK_FULL, .shift = 0};

    if(_rd_reg_getmask(ctx, regname, &m))
        regname = ctx->processorplugin->get_reg_name(m.reg, ctx->processor);

    if(!regname) return false;

    RDRegEntry e;
    if(rd_i_db_get_regval(ctx, address, regname, &e) && e.reg.has_value) {
        if(value) *value = (e.reg.value & m.mask) >> m.shift;
        return true;
    }

    return false;
}

bool rd_del_auto_regval(RDContext* ctx, RDAddress address,
                        const char* regname) {
    return _rd_del_regval(ctx, address, regname, RD_CONFIDENCE_AUTO);
}

bool rd_del_library_regval(RDContext* ctx, RDAddress address,
                           const char* regname) {
    return _rd_del_regval(ctx, address, regname, RD_CONFIDENCE_LIBRARY);
}

bool rd_del_user_regval(RDContext* ctx, RDAddress address,
                        const char* regname) {
    return _rd_del_regval(ctx, address, regname, RD_CONFIDENCE_USER);
}

bool rd_get_regval_id(RDContext* ctx, RDAddress address, RDReg id,
                      RDRegValue* value) {
    if(!ctx->processorplugin->get_reg_name) return false;

    const char* regname =
        ctx->processorplugin->get_reg_name(id, ctx->processor);
    return rd_get_regval(ctx, address, regname, value);
}

bool rd_del_auto_regval_id(RDContext* ctx, RDAddress address, RDReg id) {
    if(!ctx->processorplugin->get_reg_name) return false;

    const char* regname =
        ctx->processorplugin->get_reg_name(id, ctx->processor);
    return rd_del_auto_regval(ctx, address, regname);
}

bool rd_del_library_regval_id(RDContext* ctx, RDAddress address, RDReg id) {
    if(!ctx->processorplugin->get_reg_name) return false;

    const char* regname =
        ctx->processorplugin->get_reg_name(id, ctx->processor);
    return rd_del_library_regval(ctx, address, regname);
}

bool rd_del_user_regval_id(RDContext* ctx, RDAddress address, RDReg id) {
    if(!ctx->processorplugin->get_reg_name) return false;

    const char* regname =
        ctx->processorplugin->get_reg_name(id, ctx->processor);
    return rd_del_user_regval(ctx, address, regname);
}

RDTrackedRegSlice rd_get_all_reg(RDContext* ctx) {
    RDTrackedRegVect* regs = rd_i_db_get_reg_all(ctx, &ctx->tregs_buf);
    return vect_to_slice(RDTrackedRegSlice, regs);
}
