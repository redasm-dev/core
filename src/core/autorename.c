#include "autorename.h"
#include "io/flagsbuffer.h"
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
        bool found = false;

        for(int i = RD_MAX_OPERANDS; i-- > 0;) {
            const RDOperand* op = &instr->operands[i];

            if(op->kind == RD_OP_ADDR) {
                tgt = op->addr;
                found = true;
                break;
            }
            if(op->kind == RD_OP_MEM) {
                tgt = op->mem;
                found = true;
                break;
            }

            if(op->kind == RD_OP_REG) {
                RDRegValue val;
                if(rd_get_regval_id(ctx, instr->address, op->reg, &val)) {
                    tgt = (RDAddress)val;
                    found = true;
                    break;
                }
            }
            else if(op->kind == RD_OP_DISPL) {
                RDRegValue base, index;
                if(!rd_get_regval_id(ctx, instr->address, op->displ.base,
                                     &base))
                    break;

                RDAddress addr = (RDAddress)base;

                if(op->displ.index != RD_REGID_UNKNOWN) {
                    if(!rd_get_regval_id(ctx, instr->address, op->displ.index,
                                         &index))
                        break;

                    addr += (RDAddress)index *
                            (op->displ.scale ? op->displ.scale : 1);
                }

                tgt = addr + op->displ.offset;
                found = true;
                break;
            }
        }

        if(!found || tgt == instr->address) return;

        hasname = rd_i_get_name(ctx, tgt, false, &n);
        if(hasname) break;

        if(!rd_decode(ctx, tgt, instr)) return;
        if(instr->flow != RD_IF_JUMP) return;
    }

    if(!hasname) return;

    // target has a name, we're done
    const char* new_name = rd_i_format(namebuf, "j_%s", n.value);
    rd_auto_name(ctx, f->address, new_name);
}

static void _rd_autorename_functions(RDContext* ctx) {
    LOG_INFO("autorenaming functions");
    RDCharVect name_buf = {0};

    RDFunction** it;
    vect_each(it, &ctx->listing.functions) {
        RDFunction* f = *it;

        RDName n;
        if(rd_i_get_name(ctx, f->address, false, &n) &&
           n.confidence > RD_CONFIDENCE_AUTO)
            continue;

        RDAddress address = f->address;
        bool is_nullsub = true;
        RDInstruction instr;

        for(int i = 0; i < RD_AUTORENAME_DEPTH; i++) {
            if(!rd_decode(ctx, address, &instr)) break;

            if(instr.flow == RD_IF_JUMP) {
                is_nullsub = false;
                _rd_autorename_trampoline(ctx, f, &instr, &name_buf);
                break;
            }

            if(instr.flow == RD_IF_STOP) break;

            if(instr.flow != RD_IF_NOP)
                is_nullsub = false; // latch false, never recover

            address += instr.length;
        }

        if(is_nullsub) {
            const char* new_name = rd_i_format(&name_buf, "nullsub_%s",
                                               rd_i_to_hex(f->address, 0));
            rd_auto_name(ctx, f->address, new_name);
        }
    }

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

        const RDSegmentFull* seg = rd_i_find_segment(ctx, dst);
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
