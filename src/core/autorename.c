#include "autorename.h"
#include "io/flagsbuffer.h"
#include "support/containers.h"
#include "support/logging.h"
#include "support/utils.h"
#include <rdil/rdil.h>

#define RD_AUTORENAME_DEPTH 8

static void _rd_autorename_trampoline(RDContext* ctx, const RDFunction* f,
                                      RDAddress target, RDCharVect* namebuf) {
    RDName n;
    RDInstruction instr;
    bool found = false;

    for(int depth = 0; depth < RD_AUTORENAME_DEPTH; depth++) {
        if(rd_i_get_name(ctx, target, false, &n)) {
            const RDSegmentFull* seg = rd_i_db_find_segment(ctx, target);
            assert(seg);

            usize idx = rd_i_address2index(seg, target);
            const char* name = NULL;

            if(rd_flagsbuffer_has_imported(seg->flags, idx))
                name = rd_i_strip_prefix(n.value);
            else
                name = rd_i_format(namebuf, "j_%s", n.value);

            assert(name);
            rd_auto_name(ctx, f->address, name);
            return;
        }

        // follow chain: decode target and check for simple jump
        if(!rd_decode(ctx, target, &instr) || instr.flow != RD_IF_JUMP) return;

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

        if(!found || target == instr.address) return;
    }
}

static void _rd_autorename_functions(RDContext* ctx) {
    LOG_INFO("autorenaming functions");
    RDCharVect name_buf = {0};
    RDIL* rdil = rd_il_create(ctx, NULL);

    RDFunction** it;
    vect_each(it, &ctx->functions) {
        RDFunction* f = *it;

        RDName n;
        if(rd_i_get_name(ctx, f->address, false, &n) &&
           n.confidence > RD_CONFIDENCE_AUTO)
            continue;

        bool is_nullsub = true;
        int i = 0;
        rd_il_assign(rdil, f);

        for(; i < RD_AUTORENAME_DEPTH && rd_il_step(rdil); i++) {
            const RDInstruction* instr = rd_il_current_instr(rdil);
            assert(instr);

            RDAddress target;
            if(instr->flow == RD_IF_JUMP && rd_il_get_target(rdil, &target)) {
                is_nullsub = false;
                _rd_autorename_trampoline(ctx, f, target, &name_buf);
                break;
            }

            if(!rd_instr_is_transparent(instr))
                is_nullsub = false; // latch false, never recocver

            if(instr->flow == RD_IF_STOP) break;
        }

        if(i < RD_AUTORENAME_DEPTH && is_nullsub) {
            const char* new_name = rd_i_format(&name_buf, "nullsub_%s",
                                               rd_i_to_hex((i64)f->address, 0));
            rd_auto_name(ctx, f->address, new_name);
        }
    }

    rd_il_destroy(rdil);
    vect_destroy(&name_buf);
}

static void _rd_autorename_types(RDContext* ctx) {
    LOG_INFO("autorenaming types");

    RDCharVect name_buf = {0};

    const RDSymbol* sym;
    vect_each(sym, &ctx->listing.symbols) {
        if(sym->kind != RD_SYMBOL_TYPE) continue;

        RDTypeFull t;
        if(!rd_i_get_type(ctx, sym->address, &t) || !rd_type_is_ptr(&t.base))
            continue;

        RDAddress dst;
        if(!rd_follow_ptr(ctx, sym->address, &dst)) continue;

        const RDSegmentFull* seg = rd_i_db_find_segment(ctx, dst);
        if(!seg) continue;

        const RDFlagsBuffer* flags = seg->flags;
        usize idx = rd_i_address2index(seg, dst);
        if(!rd_i_flagsbuffer_has_xref_in(flags, idx)) continue;

        RDName target;
        if(!rd_i_get_name(ctx, dst, true, &target)) continue;

        const char* new_name = rd_i_format(&name_buf, "p_%s", target.value);
        rd_auto_name(ctx, sym->address, new_name);
    }

    vect_destroy(&name_buf);
}

void rd_i_autorename(RDContext* ctx) {
    _rd_autorename_functions(ctx);
    _rd_autorename_types(ctx);
}
