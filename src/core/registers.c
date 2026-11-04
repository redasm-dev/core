#include "registers.h"
#include "core/context.h"
#include "support/containers.h"
#include "support/hash/murmur3.h"
#include <inttypes.h>

static const char* _rd_sreg_name(RDContext* ctx, RDReg id) {
    RDQueryReg q = {
        .kind = RD_QUERY_REG_BY_ID,
        .id = id,
    };

    return rd_query_reg(ctx, &q) ? q.name : NULL;
}

// the bits 'res' covers within its root, validated: a plugin bug (empty or
// malformed mask) is reported and the register is not tracked
static bool _rd_reg_bits(const RDRegResolved* res, RDRegValue* bits) {
    RDRegValue m = res->mask.mask;
    RDRegValue run = res->mask.shift < 64 ? m >> res->mask.shift : 0;

    // non empty, starting at 'shift', contiguous
    if(!m || !(run & 1) || ((run + 1) & run)) {
        RD_LOG_FAIL("register '%s': invalid mask %" PRIx64 " (shift %u)",
                    res->name, m, (unsigned int)res->mask.shift);
        return false;
    }

    *bits = m;
    return true;
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

static const RDRegister* _rd_regmap_find(const RDRegisterHMap* map,
                                         const RDRegResolved* res,
                                         RDRegValue* bits) {
    if(!_rd_reg_bits(res, bits)) return NULL;

    RDRegister key = {.name = res->name};
    const RDRegister* r = hmap_get(map, &key);

    // a single unknown bit makes the requested register unknown
    if(!r || (r->known_bits & *bits) != *bits) return NULL;
    return r;
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

    RDRegValue bits;
    if(!_rd_reg_bits(res, &bits)) return false;

    RDRegValue v = (value << res->mask.shift) & bits;
    RDRegister key = {.name = res->name};
    RDRegister* curr = hmap_get(map, &key);

    if(!curr) {
        key.value = v;
        key.known_bits = bits;
        key.setter = setter;
        key.setter_bits = setter.has_value ? bits : 0;
        hmap_set(map, &key);
        return true;
    }

    // payload only: the hash depends on 'name'
    RDRegValue new_val = (curr->value & ~bits) | v;
    RDRegValue new_known = curr->known_bits | bits;

    curr->setter = setter;
    curr->setter_bits = setter.has_value ? bits : 0;

    if(new_val == curr->value && new_known == curr->known_bits) return false;

    curr->value = new_val;
    curr->known_bits = new_known;
    return true;
}

bool rd_i_regmap_get(const RDRegisterHMap* map, const RDRegResolved* res,
                     RDRegValue* value) {
    assert(res);

    RDRegValue bits;
    const RDRegister* r = _rd_regmap_find(map, res, &bits);
    if(!r) return false;

    if(value) *value = (r->value & bits) >> res->mask.shift;
    return true;
}

bool rd_i_regmap_del(RDRegisterHMap* map, const RDRegResolved* res) {
    assert(res);

    RDRegValue bits;
    if(!_rd_reg_bits(res, &bits)) return false;

    RDRegister key = {.name = res->name};
    RDRegister* r = hmap_get(map, &key);
    if(!r) return true;

    r->known_bits &= ~bits;
    r->value &= ~bits;
    r->setter = (RDRegSetter){0};
    r->setter_bits = 0;

    if(!r->known_bits) hmap_del(map, &key);
    return true;
}

bool rd_i_regmap_get_info(const RDRegisterHMap* map, const RDRegResolved* res,
                          RDRegValueInfo* info) {
    assert(res);

    RDRegValue bits;
    const RDRegister* r = _rd_regmap_find(map, res, &bits);
    if(!r) return false;

    if(info) {
        info->value = (r->value & bits) >> res->mask.shift;

        if(r->setter.has_value && (r->setter_bits & bits) == bits)
            info->setter = r->setter;
        else
            info->setter = (RDRegSetter){0};
    }

    return true;
}

bool rd_set_regval(RDContext* ctx, const char* regname, u64 value) {
    RDRegResolved res;
    return rd_i_reg_resolve_name(ctx, regname, &res) &&
           rd_i_regmap_set(&ctx->engine.current.registers, &res, value,
                           rd_i_current_setter(ctx));
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
                           rd_i_current_setter(ctx));
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

        rd_auto_xref_add(ctx, info.setter.value, (RDAddress)info.value,
                         RD_DR_ADDRESS);
        return true;
    }

    return false;
}
