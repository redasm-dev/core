#include "argdiscover.h"
#include "io/flagsbuffer.h"
#include "rdil/rdil.h"
#include "support/containers.h"
#include <inttypes.h>

static bool _rd_discover_arg_is_address(const RDOperand* op, RDAddress* out) {
    switch(op->kind) {
        case RD_OP_ADDR: *out = op->addr; return true;
        case RD_OP_IMM: *out = (RDAddress)op->imm; return true;
        default: return false;
    }
}

static usize _rd_discover_arg_depth(const RDCallConv* cc, usize index,
                                    usize n_args) {
    return cc->arg_order == RD_ARGORDER_RTL ? index : n_args - 1 - index;
}

static bool _rd_discover_can_step_back(const RDContext* ctx,
                                       RDAddress address) {
    const RDSegmentFull* seg = rd_i_db_find_segment(ctx, address);
    if(!seg) return false;

    usize idx = rd_i_address2index(seg, address);

    return rd_flagsbuffer_has_flow(seg->flags, idx) &&
           !rd_i_flagsbuffer_has_jmpdst(seg->flags, idx);
}

static bool _rd_discover_get_call_arg(RDContext* ctx, RDAddress call_address,
                                      const RDCallConv* cc, usize index,
                                      usize n_args, RDOperand* out,
                                      RDInstructionVect* il_vect) {
    if(index < vect_length(&cc->arg_regs)) return false;

    usize depth = _rd_discover_arg_depth(cc, index, n_args);
    RDAddress addr = call_address;
    usize npushes = 0;

    while(_rd_discover_can_step_back(ctx, addr)) {
        const RDInstructionVect* il = rd_i_il_lift_prev(ctx, addr, il_vect);
        if(!il) return false;

        addr = il->real_instr.address;

        // Control flow ends the walk.
        if(rd_instr_is_branch(&il->real_instr) ||
           il->real_instr.flow == RD_IF_STOP)
            return false;

        assert(!vect_is_empty(il));
        if(vect_length(il) != 1) continue;

        const RDInstruction* il_instr = vect_first(il);

        if(il_instr->id == RD_IL_UNKNOWN || // can't classify
           il_instr->id == RD_IL_POP)       // pop shifts the stack
            return false;

        if(il_instr->id != RD_IL_PUSH) continue; // skipped, not counted

        if(npushes++ == depth) {
            *out = il_instr->operands[0];
            return true;
        }
    }

    return false;
}

static void _rd_discover_call_args(RDContext* ctx, const RDFunction* f,
                                   RDAddress call_address,
                                   RDInstructionVect* il_vect,
                                   RDCharVect* fmt_buf) {
    const RDTypeDef* tdef = f->type_def;
    const RDCallConv* cc = rd_i_callconv_find(ctx, tdef->func_.callconv);
    if(!cc) return; // names a convention this processor never registered

    usize n_args = vect_length(&tdef->func_.args.value);

    for(usize i = 0; i < n_args; i++) {
        const RDParam* p = vect_at(&tdef->func_.args.value, i);
        if(!p->type.def || p->type.def->kind != RD_TKIND_FUNC || p->type.count)
            continue;

        RDOperand op;
        if(!_rd_discover_get_call_arg(ctx, call_address, cc, i, n_args, &op,
                                      il_vect))
            continue;

        RDAddress fn;
        if(!_rd_discover_arg_is_address(&op, &fn)) continue;

        // The argument has to point at something executable. A literal
        // sitting in a function-typed slot but landing in data is a null
        // handle or a sentinel, not a callback.
        const RDSegment* seg = rd_find_segment(ctx, fn);
        if(!seg || !(seg->perm & RD_SP_X)) continue;

        rd_set_function(ctx, fn);
        rd_auto_name(ctx, fn, rd_i_format(fmt_buf, "%s_%" PRIx64, p->name, fn));
    }
}

void rd_i_discover_args(RDContext* ctx) {
    RDCharVect fmt_buf = {0};
    RDXRefVect xrefs = {0};
    RDInstructionVect il = {0};

    RDFunction** it;
    vect_each(it, &ctx->functions) {
        const RDFunction* f = *it;

        // A prototype with real arguments AND a declared convention.
        // args.has_value alone isn't enough: `callconv` is an optional KB
        // key, and without one there's no way to know where an argument
        // lives.
        // The builtin `function` placeholder falls out here too.
        // It never sets args.has_value.

        if(!f->type_def) continue;
        assert(f->type_def->kind == RD_TKIND_FUNC);

        if(!f->type_def->func_.args.has_value || !f->type_def->func_.callconv)
            continue;

        rd_i_get_xrefs_to_ex(ctx, f->address, RD_XR_NONE, &xrefs);

        const RDXRef* r;
        vect_each(r, &xrefs) {
            if(r->type != RD_CR_CALL) continue;
            _rd_discover_call_args(ctx, f, r->address, &il, &fmt_buf);
        }
    }

    vect_destroy(&il);
    vect_destroy(&xrefs);
    vect_destroy(&fmt_buf);
}
