#include "autorename.h"
#include "support/containers.h"
#include "support/logging.h"

#define RD_AUTORENAME_DEPTH 8

static void _rd_autorename_trampoline(RDContext* ctx, const RDFunction* f,
                                      RDInstruction* instr,
                                      RDCharVect* namebuf) {
    RDName n;
    RDAddress tgt;
    bool hasname = false;

    for(int depth = 0; depth < RD_AUTORENAME_DEPTH; depth++) {
        if(instr->operands[0].kind == RD_OP_ADDR)
            tgt = instr->operands[0].addr;
        else if(instr->operands[0].kind == RD_OP_MEM)
            tgt = instr->operands[0].mem;
        else
            return;

        // ignore self loops
        if(tgt == instr->address) return;

        // target has a name, we're done
        hasname = rd_i_get_name(ctx, tgt, false, &n);
        if(hasname) break;

        // target is unnamed, follow one more jump if possible
        if(!rd_decode(ctx, tgt, instr)) return;
        if(instr->flow != RD_IF_JUMP) return;
    }

    if(!hasname) return;

    const char* new_name = rd_i_format(namebuf, "j_%s", n.value);
    rd_auto_name(ctx, f->address, new_name);
}

static void _rd_autorename_nullsub(RDContext* ctx, const RDFunction* f,
                                   RDInstruction* instr, RDCharVect* namebuf) {
    bool is_nullsub = false;
    RDAddress addr = f->address;

    while(rd_decode(ctx, addr, instr)) {
        if(instr->flow == RD_IF_STOP) {
            is_nullsub = true;
            break;
        }

        if(instr->flow != RD_IF_NOP) break; // disqualified

        addr += instr->length;
    }

    if(!is_nullsub) return;

    const char* new_name =
        rd_i_format(namebuf, "nullsub_%s", rd_i_to_hex(f->address, 0));

    rd_auto_name(ctx, f->address, new_name);
}

void rd_i_autorename(RDContext* ctx) {
    LOG_INFO("autorenaming functions");
    RDCharVect name_buf = {0};

    RDFunction** it;
    vect_each(it, &ctx->listing.functions) {
        RDFunction* f = *it;

        RDName n;
        if(rd_i_get_name(ctx, f->address, false, &n) &&
           n.confidence > RD_CONFIDENCE_AUTO)
            continue;

        RDInstruction instr;
        if(!rd_decode(ctx, f->address, &instr)) continue;

        if(instr.flow == RD_IF_JUMP)
            _rd_autorename_trampoline(ctx, f, &instr, &name_buf);
        else
            _rd_autorename_nullsub(ctx, f, &instr, &name_buf);
    }

    vect_destroy(&name_buf);
}
