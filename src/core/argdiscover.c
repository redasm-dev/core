#include "argdiscover.h"
#include "io/flagsbuffer.h"
#include "rdil/rdil.h"
#include "support/containers.h"
#include <inttypes.h>

typedef struct RDArgTarget {
    RDAddress address;
    const RDTypeDef* tdef;
    const RDCallConv* cc;
} RDArgTarget;

typedef struct RDArgTargetVect {
    RDArgTarget* data;
    usize length;
    usize capacity;
} RDArgTargetVect;

static bool _rd_discover_is_call_target(const RDContext* ctx,
                                        const RDTypeDef* tdef,
                                        const RDCallConv** cc) {
    if(!tdef || (tdef->kind != RD_TKIND_FUNC) || rd_typedef_is_proto(tdef))
        return false;

    if(!tdef->func_.args.has_value || vect_is_empty(&tdef->func_.args.value))
        return false;

    *cc = rd_i_callconv_find(ctx, tdef->func_.callconv);
    return *cc != NULL;
}

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

static void _rd_discover_call_args(RDContext* ctx, const RDTypeDef* tdef,
                                   RDAddress call_address, const RDCallConv* cc,
                                   RDInstructionVect* il_vect,
                                   RDCharVect* fmt_buf) {
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
        const RDSegmentFull* seg = rd_i_db_find_segment(ctx, fn);
        if(!seg || !(seg->base.perm & RD_SP_X)) continue;

        rd_set_typed_function(ctx, fn, p->type.def->name);
        rd_auto_name(ctx, fn, rd_i_format(fmt_buf, "%s_%" PRIx64, p->name, fn));
    }
}

static void _rd_discover_collect_targets(RDContext* ctx,
                                         RDArgTargetVect* targets) {
    const RDTypeDef* builtin = rd_i_typedef_find(ctx, "function");

    // (1) thunks: prototype attached to a function by autorename
    RDFunction** fit;
    vect_each(fit, &ctx->functions) {
        const RDFunction* f = *fit;
        const RDCallConv* cc = NULL;

        // The builtin placeholder: "it's a function, signature unknown".
        // Not redundant with the args check below: that one rejects it only
        // because the builtin happens to have no args, which is incidental.
        if(f->type_def == builtin) continue;
        if(!_rd_discover_is_call_target(ctx, f->type_def, &cc)) continue;

        vect_push(targets, ((RDArgTarget){
                               .address = f->address,
                               .tdef = f->type_def,
                               .cc = cc,
                           }));
    }

    // (2) direct indirect: prototype attached to nothing, resolve the
    //     named address instead
    RDTypeDef** tit;
    vect_each(tit, &ctx->typedefs) {
        const RDTypeDef* tdef = *tit;
        const RDCallConv* cc = NULL;
        if(!_rd_discover_is_call_target(ctx, tdef, &cc)) continue;

        RDAddress address;
        if(!rd_get_address(ctx, tdef->name, &address)) continue;

        // (1) already covers this:
        //     a function at this exact address already carries the prototype.
        const RDFunction* f = rd_i_get_function(ctx, address);
        if(f && f->type_def == tdef) continue;

        vect_push(targets, ((RDArgTarget){
                               .address = address,
                               .tdef = tdef,
                               .cc = cc,
                           }));
    }
}

void rd_i_discover_args(RDContext* ctx) {
    RDCharVect fmt_buf = {0};
    RDXRefVect xrefs = {0};
    RDInstructionVect il = {0};
    RDArgTargetVect targets = {0};

    _rd_discover_collect_targets(ctx, &targets);

    const RDArgTarget* t;
    vect_each(t, &targets) {
        rd_i_get_xrefs_to_ex(ctx, t->address, RD_XR_NONE, &xrefs);

        const RDXRef* r;
        vect_each(r, &xrefs) {
            RDInstruction instr;
            if(!rd_decode(ctx, r->address, &instr)) continue;
            if(!rd_instr_is_call(&instr)) continue;

            _rd_discover_call_args(ctx, t->tdef, r->address, t->cc, &il,
                                   &fmt_buf);
        }
    }

    vect_destroy(&targets);
    vect_destroy(&il);
    vect_destroy(&xrefs);
    vect_destroy(&fmt_buf);
}
