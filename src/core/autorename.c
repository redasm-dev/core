#include "autorename.h"
#include "io/flagsbuffer.h"
#include "support/containers.h"
#include "support/utils.h"
#include <rdil/rdil.h>
#include <redasm/support/logging.h>

#define RD_AUTORENAME_DEPTH 8

static const char* _rd_resolve_name(RDContext* ctx, RDAddress target,
                                    RDAddress* resolved, RDConfidence* c) {
    RDName n;
    RDInstruction instr;
    bool found = false;

    for(int depth = 0; depth < RD_AUTORENAME_DEPTH; depth++) {
        if(rd_i_get_name(ctx, target, false, &n)) {
            if(resolved) *resolved = target;
            if(c) *c = n.confidence;

            str_clear(&ctx->autoname_buf);
            str_append(&ctx->autoname_buf, n.value);
            return ctx->autoname_buf.data;
        }

        // follow chain: decode target and check for simple jump
        if(!rd_decode(ctx, target, &instr) || instr.flow != RD_IF_JUMP)
            return NULL;

        found = false;

        // only follow RD_OP_ADDR and RD_OP_MEM, no register tracking here
        for(int i = RD_MAX_OPERANDS; i-- > 0;) {
            const RDOperand* op = &instr.operands[i];

            if(op->kind == RD_OP_ADDR) {
                target = op->addr;
                found = true;
                break;
            }

            if(op->kind == RD_OP_MEM) {
                target = op->mem;
                found = true;
                break;
            }
        }

        if(!found || target == instr.address) return NULL;
    }

    return NULL;
}

static void _rd_autorename_functions(RDContext* ctx) {
    RD_LOG_INFO("autorenaming functions");
    RDCharVect namebuf = {0};
    RDIL* rdil = rd_il_create(ctx, NULL);

    RDFunction** it;
    vect_each(it, &ctx->functions) {
        RDFunction* f = *it;

        RDName n;
        if(rd_i_get_name(ctx, f->address, false, &n) &&
           n.confidence > RD_CONFIDENCE_AUTO)
            continue;

        rd_il_assign(rdil, f);

        RDAddress resolved;
        RDConfidence c;
        const char* resolved_name = rd_i_resolve_name(ctx, rdil, &resolved, &c);
        if(!resolved_name) continue;

        const RDTypeDef* tdef = rd_i_typedef_find(ctx, resolved_name);
        if(tdef && tdef->kind == RD_TKIND_FUNC)
            rd_i_function_set_type_def(f, tdef);

        const RDSegmentFull* seg = rd_i_db_find_segment(ctx, resolved);
        assert(seg);

        usize idx = rd_i_address2index(seg, resolved);
        const char* auto_name = NULL;

        if(rd_flagsbuffer_has_imported(seg->flags, idx))
            auto_name = rd_i_format(&namebuf, "imp_%s", resolved_name);
        else if(rd_flagsbuffer_has_exported(seg->flags, idx))
            auto_name = rd_i_format(&namebuf, "exp_%s", resolved_name);
        else
            auto_name = rd_i_format(&namebuf, "j_%s", resolved_name);

        assert(auto_name);
        rd_i_set_name(ctx, f->address, auto_name, c);
    }

    rd_il_destroy(rdil);
    vect_destroy(&namebuf);
}

static void _rd_autorename_types(RDContext* ctx) {
    RD_LOG_INFO("autorenaming types");

    RDCharVect name_buf = {0};
    RDAddressVect addresses = {0};
    RDTypeVect types = {0};
    rd_i_db_get_all_types(ctx, &addresses, &types);

    for(usize i = 0; i < vect_length(&addresses); i++) {
        const RDType* t = vect_at(&types, i);
        if(!rd_type_is_ptr(t)) continue;

        RDAddress address = *vect_at(&addresses, i);

        RDAddress dst;
        if(!rd_follow_ptr(ctx, address, &dst)) continue;

        const RDSegmentFull* seg = rd_i_db_find_segment(ctx, dst);
        if(!seg) continue;

        const RDFlagsBuffer* flags = seg->flags;
        usize idx = rd_i_address2index(seg, dst);
        if(!rd_i_flagsbuffer_has_xref_in(flags, idx)) continue;

        RDName target;
        if(!rd_i_get_name(ctx, dst, true, &target)) continue;

        const char* new_name = rd_i_format(&name_buf, "p_%s", target.value);
        rd_auto_name(ctx, address, new_name);
    }

    vect_destroy(&types);
    vect_destroy(&addresses);
    vect_destroy(&name_buf);
}

const char* rd_i_resolve_name(RDContext* ctx, RDIL* rdil, RDAddress* resolved,
                              RDConfidence* c) {
    for(int i = 0; i < RD_AUTORENAME_DEPTH && rd_il_step(rdil); i++) {
        const RDInstruction* instr = rd_il_current_instr(rdil);
        assert(instr);

        RDAddress target;
        if(instr->flow == RD_IF_JUMP && rd_il_get_target(rdil, &target))
            return _rd_resolve_name(ctx, target, resolved, c);

        if(instr->flow == RD_IF_STOP) break;
    }

    return NULL;
}

void rd_i_autorename(RDContext* ctx) {
    _rd_autorename_functions(ctx);
    _rd_autorename_types(ctx);
}

const char* rd_resolve_name(RDContext* self, RDAddress address,
                            RDAddress* resolved) {
    RDName n;
    if(rd_i_get_name(self, address, false, &n)) {
        if(resolved) *resolved = address;
        return n.value;
    }

    const RDFunction* f = rd_find_function(self, address);
    if(!f) return NULL;

    RDIL* rdil = rd_il_create(self, f);
    const char* name = rd_i_resolve_name(self, rdil, resolved, NULL);
    rd_il_destroy(rdil);
    return name;
}
