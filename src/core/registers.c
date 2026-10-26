#include "registers.h"
#include "core/context.h"
#include "core/worker.h"
#include "support/containers.h"
#include "support/hash/murmur3.h"

static const char* _rd_sreg_name(RDContext* ctx, RDReg id) {
    RDQueryReg q = {
        .kind = RD_QUERY_REG_BY_ID,
        .id = id,
    };

    return rd_query_reg(ctx, &q) ? q.name : NULL;
}

static RDRegSetter _rd_current_setter(const RDContext* ctx) {
    // outside the emulate step there is no current instruction: initial
    // register state set by a loader must not look like a definition
    if(ctx->engine.step != RD_WS_EMULATE) return (RDRegSetter){0};

    return (RDRegSetter){
        .value = ctx->engine.current.address,
        .has_value = true,
    };
}

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

void rd_i_registermap_init(RDRegisterHMap* self) {
    *self = (RDRegisterHMap){
        .hash = _rd_register_hash,
        .equal = _rd_register_equal,
    };
}

bool rd_i_reg_resolve_name(RDContext* ctx, const char* regname,
                           RDRegResolved* out) {
    if(!regname) return false;

    RDQueryReg q = {
        .kind = RD_QUERY_REG_BY_NAME,
        .want = RD_QUERY_REG_WANT_MASK | RD_QUERY_REG_WANT_ROOT,
        .name = regname,
    };

    if(!rd_query_reg(ctx, &q)) return false;

    out->mask = q.mask;
    out->name = rd_i_strpool_intern(&ctx->strings, q.root_name);
    return true;
}

bool rd_i_reg_resolve_id(RDContext* ctx, RDReg id, RDRegResolved* out) {
    if(id == RD_REGID_INVALID) return false;

    RDQueryReg q = {
        .kind = RD_QUERY_REG_BY_ID,
        .want = RD_QUERY_REG_WANT_MASK | RD_QUERY_REG_WANT_ROOT,
        .id = id,
    };

    if(!rd_query_reg(ctx, &q)) return false;

    out->mask = q.mask;
    out->name = rd_i_strpool_intern(&ctx->strings, q.root_name);
    return true;
}

bool rd_i_regmap_set(RDRegisterHMap* map, const RDRegResolved* res,
                     RDRegValue value, RDRegSetter setter) {
    assert(res);

    RDRegister r = {.name = res->name};
    RDRegister* curr = hmap_get(map, &r);
    bool is_full = res->mask.mask == RD_REGMASK_FULL;

    if(!is_full) { // merge partial write
        RDRegValue base = curr ? curr->value : 0;
        value = (base & ~res->mask.mask) |
                ((value << res->mask.shift) & res->mask.mask);
    }

    if(curr) {
        if(is_full) curr->setter = setter;
        if(curr->value == value) return false;
        curr->value = value;
        return true;
    }

    if(is_full) r.setter = setter;
    r.value = value;
    hmap_set(map, &r);
    return true;
}

bool rd_i_regmap_get(const RDRegisterHMap* map, const RDRegResolved* res,
                     RDRegValue* value) {
    assert(res);

    RDRegister key = {.name = res->name};

    const RDRegister* r = hmap_get(map, &key);
    if(!r) return false;

    if(value) *value = (r->value & res->mask.mask) >> res->mask.shift;
    return true;
}

bool rd_i_regmap_del(RDRegisterHMap* map, const RDRegResolved* res) {
    RDRegister key = {.name = res->name};
    hmap_del(map, &key);
    return true;
}

bool rd_i_regmap_get_info(const RDRegisterHMap* map, const RDRegResolved* res,
                          RDRegValueInfo* info) {
    RDRegister key = {.name = res->name};
    const RDRegister* r = hmap_get(map, &key);
    if(!r) return false;

    if(info) {
        info->value = (r->value & res->mask.mask) >> res->mask.shift;
        info->setter = r->setter;
    }

    return true;
}

bool rd_set_regval(RDContext* ctx, const char* regname, u64 value) {
    RDRegResolved res;
    return rd_i_reg_resolve_name(ctx, regname, &res) &&
           rd_i_regmap_set(&ctx->engine.current.registers, &res, value,
                           _rd_current_setter(ctx));
}

bool rd_get_regval(RDContext* ctx, const char* regname, RDRegValue* value) {
    RDRegResolved res;
    return rd_i_reg_resolve_name(ctx, regname, &res) &&
           rd_i_regmap_get(&ctx->engine.current.registers, &res, value);
}

bool rd_del_regval(RDContext* ctx, const char* regname) {
    RDRegResolved res;
    return rd_i_reg_resolve_name(ctx, regname, &res) &&
           rd_i_regmap_del(&ctx->engine.current.registers, &res);
}

bool rd_set_regval_id(RDContext* ctx, RDReg id, RDRegValue value) {
    RDRegResolved res;
    return rd_i_reg_resolve_id(ctx, id, &res) &&
           rd_i_regmap_set(&ctx->engine.current.registers, &res, value,
                           _rd_current_setter(ctx));
}

bool rd_get_regval_id(RDContext* ctx, RDReg id, RDRegValue* value) {
    RDRegResolved res;
    return rd_i_reg_resolve_id(ctx, id, &res) &&
           rd_i_regmap_get(&ctx->engine.current.registers, &res, value);
}

bool rd_del_regval_id(RDContext* ctx, RDReg id) {
    RDRegResolved res;
    return rd_i_reg_resolve_id(ctx, id, &res) &&
           rd_i_regmap_del(&ctx->engine.current.registers, &res);
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
    const char* regname = _rd_sreg_name(ctx, id);
    return regname && rd_auto_sregval(ctx, address, regname, value);
}

bool rd_library_sregval_id(RDContext* ctx, RDAddress address, RDReg id,
                           RDRegValue value) {
    const char* regname = _rd_sreg_name(ctx, id);
    return regname && rd_library_sregval(ctx, address, regname, value);
}

bool rd_user_sregval_id(RDContext* ctx, RDAddress address, RDReg id,
                        RDRegValue value) {
    const char* regname = _rd_sreg_name(ctx, id);
    return regname && rd_user_sregval(ctx, address, regname, value);
}

bool rd_del_auto_sregval_id(RDContext* ctx, RDAddress address, RDReg id) {
    const char* regname = _rd_sreg_name(ctx, id);
    return regname && rd_del_auto_sregval(ctx, address, regname);
}

bool rd_del_library_sregval_id(RDContext* ctx, RDAddress address, RDReg id) {
    const char* regname = _rd_sreg_name(ctx, id);
    return regname && rd_del_library_sregval(ctx, address, regname);
}

bool rd_del_user_sregval_id(RDContext* ctx, RDAddress address, RDReg id) {
    const char* regname = _rd_sreg_name(ctx, id);
    return regname && rd_del_user_sregval(ctx, address, regname);
}

bool rd_get_sregval_id(RDContext* ctx, RDAddress address, RDReg id,
                       RDRegValue* value) {
    const char* regname = _rd_sreg_name(ctx, id);
    return regname && rd_get_sregval(ctx, address, regname, value);
}

bool rd_get_regval_info(RDContext* ctx, const char* regname,
                        RDRegValueInfo* info) {
    RDRegResolved res;
    return rd_i_reg_resolve_name(ctx, regname, &res) &&
           rd_i_regmap_get_info(&ctx->engine.current.registers, &res, info);
}

bool rd_get_regval_info_id(RDContext* ctx, RDReg id, RDRegValueInfo* info) {
    RDRegResolved res;
    return rd_i_reg_resolve_id(ctx, id, &res) &&
           rd_i_regmap_get_info(&ctx->engine.current.registers, &res, info);
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

bool rd_mark_regval_address(RDContext* ctx, RDReg id) {
    RDRegValueInfo info;
    if(!rd_get_regval_info_id(ctx, id, &info)) return false;

    // a merged or externally set value has no instruction
    if(!info.setter.has_value) return false;
    if(!rd_is_address(ctx, (RDAddress)info.value)) return false;

    RDInstruction setter;
    if(!rd_decode(ctx, info.setter.value, &setter)) return false;

    rd_foreach_operand(i, op, &setter) {
        if(op->kind != RD_OP_IMM || op->imm != info.value) continue;

        rd_add_xref(ctx, info.setter.value, (RDAddress)info.value,
                    RD_DR_ADDRESS);
        return true;
    }

    return false;
}
