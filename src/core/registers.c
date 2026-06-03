#include "registers.h"
#include "core/context.h"
#include "support/containers.h"
#include "support/hash.h"

static size_t _rd_register_hash(const void* e) {
    const RDRegister* r = (const RDRegister*)e;
    return rd_i_murmur3(r->name, (u32)strlen(r->name));
}

static bool _rd_register_equal(const void* a, const void* b) {
    const RDRegister* ra = (const RDRegister*)a;
    const RDRegister* rb = (const RDRegister*)b;
    if(ra->name == rb->name) return true;
    return !strcmp(ra->name, rb->name);
}

static bool _rd_reg_getmask(RDContext* ctx, const char* regname, RDRegMask* m) {
    if(ctx->processorplugin->get_reg_mask) {
        if(!ctx->processorplugin->get_reg_name) return false;

        if(!ctx->processorplugin->get_reg_mask(regname, m, ctx->processor))
            return false;

        return ctx->processorplugin->get_reg_name(m->reg, ctx->processor);
    }

    return false;
}

void rd_i_registermap_init(RDRegisterHMap* self) {
    *self = (RDRegisterHMap){
        .hash = _rd_register_hash,
        .equal = _rd_register_equal,
    };
}

bool rd_set_regval(RDContext* ctx, const char* regname, u64 value) {
    if(!regname) return false;

    RDRegMask m = {.mask = RD_REGMASK_FULL, .shift = 0};

    if(_rd_reg_getmask(ctx, regname, &m))
        regname = ctx->processorplugin->get_reg_name(m.reg, ctx->processor);

    if(!regname) return false;

    RDRegister r = {
        .name = rd_i_strpool_intern(&ctx->strings, regname),
    };

    // merge if bit field
    if(m.mask != RD_REGMASK_FULL) {
        RDRegValue base = 0;

        const RDRegister* curr = hmap_get(&ctx->engine.current.registers, &r);
        if(curr) base = curr->value;

        value = (base & ~m.mask) | ((value << m.shift) & m.mask);

        if(curr && curr->value == value)
            return false; // same value, don't update
    }

    r.value = value;
    hmap_set(&ctx->engine.current.registers, &r);
    return true;
}

bool rd_get_regval(RDContext* ctx, const char* regname, RDRegValue* value) {
    if(!regname) return false;

    RDRegMask m = {.mask = RD_REGMASK_FULL, .shift = 0};

    if(_rd_reg_getmask(ctx, regname, &m))
        regname = ctx->processorplugin->get_reg_name(m.reg, ctx->processor);

    if(!regname) return false;

    RDRegister key = {
        .name = rd_i_strpool_intern(&ctx->strings, regname),
    };

    const RDRegister* r = hmap_get(&ctx->engine.current.registers, &key);
    if(!r) return false;

    if(value) *value = (r->value & m.mask) >> m.shift;
    return true;
}

bool rd_del_regval(RDContext* ctx, const char* regname) {
    if(!regname) return false;

    RDRegMask m = {.mask = RD_REGMASK_FULL, .shift = 0};

    if(_rd_reg_getmask(ctx, regname, &m))
        regname = ctx->processorplugin->get_reg_name(m.reg, ctx->processor);

    if(!regname) return false;

    RDRegister key = {
        .name = rd_i_strpool_intern(&ctx->strings, regname),
    };

    hmap_del(&ctx->engine.current.registers, &key);
    return true;
}

bool rd_set_regval_id(RDContext* ctx, RDReg id, RDRegValue value) {
    if(!ctx->processorplugin->get_reg_name) return false;

    const char* regname =
        ctx->processorplugin->get_reg_name(id, ctx->processor);
    return regname && rd_set_regval(ctx, regname, value);
}

bool rd_get_regval_id(RDContext* ctx, RDReg id, RDRegValue* value) {
    if(!ctx->processorplugin->get_reg_name) return false;

    const char* regname =
        ctx->processorplugin->get_reg_name(id, ctx->processor);
    return regname && rd_get_regval(ctx, regname, value);
}

bool rd_del_regval_id(RDContext* ctx, RDReg id) {
    if(!ctx->processorplugin->get_reg_name) return false;

    const char* regname =
        ctx->processorplugin->get_reg_name(id, ctx->processor);
    return regname && rd_del_regval(ctx, regname);
}

bool rd_auto_sregval(RDContext* ctx, RDAddress address, const char* regname,
                     RDRegValue value) {
    return rd_i_db_set_sregval(ctx, address, regname, value,
                               RD_CONFIDENCE_AUTO);
}

bool rd_library_sregval(RDContext* ctx, RDAddress address, const char* regname,
                        RDRegValue value) {
    return rd_i_db_set_sregval(ctx, address, regname, value,
                               RD_CONFIDENCE_LIBRARY);
}

bool rd_user_sregval(RDContext* ctx, RDAddress address, const char* regname,
                     RDRegValue value) {
    return rd_i_db_set_sregval(ctx, address, regname, value,
                               RD_CONFIDENCE_USER);
}

bool rd_del_auto_sregval(RDContext* ctx, RDAddress address,
                         const char* regname) {
    return rd_i_db_del_sregval(ctx, address, regname, RD_CONFIDENCE_AUTO);
}

bool rd_del_library_sregval(RDContext* ctx, RDAddress address,
                            const char* regname) {
    return rd_i_db_del_sregval(ctx, address, regname, RD_CONFIDENCE_LIBRARY);
}

bool rd_del_user_sregval(RDContext* ctx, RDAddress address,
                         const char* regname) {
    return rd_i_db_del_sregval(ctx, address, regname, RD_CONFIDENCE_USER);
}

bool rd_get_sregval(RDContext* ctx, RDAddress address, const char* regname,
                    RDRegValue* value) {
    return rd_i_db_get_sregval(ctx, address, regname, value);
}

bool rd_auto_sregval_id(RDContext* ctx, RDAddress address, RDReg id,
                        RDRegValue value) {
    if(!ctx->processorplugin->get_reg_name) return false;

    const char* regname =
        ctx->processorplugin->get_reg_name(id, ctx->processor);
    return regname && rd_auto_sregval(ctx, address, regname, value);
}

bool rd_library_sregval_id(RDContext* ctx, RDAddress address, RDReg id,
                           RDRegValue value) {
    if(!ctx->processorplugin->get_reg_name) return false;

    const char* regname =
        ctx->processorplugin->get_reg_name(id, ctx->processor);
    return regname && rd_library_sregval(ctx, address, regname, value);
}

bool rd_user_sregval_id(RDContext* ctx, RDAddress address, RDReg id,
                        RDRegValue value) {
    if(!ctx->processorplugin->get_reg_name) return false;

    const char* regname =
        ctx->processorplugin->get_reg_name(id, ctx->processor);
    return regname && rd_user_sregval(ctx, address, regname, value);
}

bool rd_del_auto_sregval_id(RDContext* ctx, RDAddress address, RDReg id) {
    if(!ctx->processorplugin->get_reg_name) return false;

    const char* regname =
        ctx->processorplugin->get_reg_name(id, ctx->processor);
    return regname && rd_del_auto_sregval(ctx, address, regname);
}

bool rd_del_library_sregval_id(RDContext* ctx, RDAddress address, RDReg id) {
    if(!ctx->processorplugin->get_reg_name) return false;

    const char* regname =
        ctx->processorplugin->get_reg_name(id, ctx->processor);
    return regname && rd_del_library_sregval(ctx, address, regname);
}

bool rd_del_user_sregval_id(RDContext* ctx, RDAddress address, RDReg id) {
    if(!ctx->processorplugin->get_reg_name) return false;

    const char* regname =
        ctx->processorplugin->get_reg_name(id, ctx->processor);
    return regname && rd_del_user_sregval(ctx, address, regname);
}

bool rd_get_sregval_id(RDContext* ctx, RDAddress address, RDReg id,
                       RDRegValue* value) {
    if(!ctx->processorplugin->get_reg_name) return false;

    const char* regname =
        ctx->processorplugin->get_reg_name(id, ctx->processor);
    return regname && rd_get_sregval(ctx, address, regname, value);
}

RDSegmentRegNameSlice rd_get_all_sreg_names(const RDContext* ctx) {
    return vect_to_slice(RDSegmentRegNameSlice,
                         rd_i_db_get_all_sreg_names(ctx));
}

RDSegmentRegSlice rd_get_all_sregval(RDContext* ctx, const char* regname) {
    const RDSegmentRegVect* v = rd_i_db_get_all_sregval(ctx, regname);
    if(v) return vect_to_slice(RDSegmentRegSlice, v);
    return (RDSegmentRegSlice){0};
}
