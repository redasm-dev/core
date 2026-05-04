#include "processor.h"
#include "core/context.h"
#include "surface/renderer.h"
#include <stddef.h>

static bool _rd_render_operand_default(RDRenderer* r,
                                       const RDInstruction* instr, usize idx,
                                       RDProcessor* p) {
    RD_UNUSED(p);
    const RDProcessorPlugin* plugin = r->context->processorplugin;
    const RDOperand* op = &instr->operands[idx];
    const unsigned int ADDR_W = plugin->ptr_size * 2;

    switch(op->kind) {
        case RD_OP_CNST:
            rd_renderer_num(r, op->imm, 16, 0, RD_NUM_NOADDR);
            break;

        case RD_OP_REG: rd_renderer_reg(r, op->reg); break;

        case RD_OP_IMM:
            rd_renderer_num(r, op->imm, 16, 0, RD_NUM_DEFAULT);
            break;

        case RD_OP_ADDR:
            rd_renderer_loc(r, op->addr, ADDR_W, RD_NUM_DEFAULT);
            break;

        case RD_OP_MEM:
            rd_renderer_norm(r, "[");
            rd_renderer_loc(r, op->mem, ADDR_W, RD_NUM_DEFAULT);
            rd_renderer_norm(r, "]");
            break;

        case RD_OP_PHRASE: {
            rd_renderer_norm(r, "[");
            rd_renderer_reg(r, op->phrase.base);

            if(op->phrase.index != RD_REGID_UNKNOWN) {
                rd_renderer_norm(r, "+");
                rd_renderer_reg(r, op->phrase.index);
            }

            rd_renderer_norm(r, "]");
            break;
        }

        case RD_OP_DISPL: {
            rd_renderer_norm(r, "[");
            rd_renderer_reg(r, op->displ.base);
            rd_renderer_norm(r, "+");
            rd_renderer_reg(r, op->displ.index);

            if(op->displ.scale > 1) {
                rd_renderer_norm(r, "*");
                rd_renderer_num(r, op->displ.scale, 16, 0, RD_NUM_DEFAULT);
            }

            if(op->displ.offset != 0)
                rd_renderer_num(r, op->displ.offset, 16, 0, RD_NUM_SIGNED);

            rd_renderer_norm(r, "]");
            break;
        }

        default: rd_renderer_muted(r, "???"); break;
    }

    return true;
}

void rd_i_processor_render_instruction(RDRenderer* r,
                                       const RDInstruction* instr) {
    const RDContext* ctx = r->context;
    const RDProcessorPlugin* p = ctx->processorplugin;

    bool done = false;
    if(p->render_mnemonic) done = p->render_mnemonic(r, instr, ctx->processor);
    if(!done) rd_renderer_mnem(r, instr, RD_THEME_DEFAULT);

    rd_renderer_ws(r, 1);

    RDCellData* data = rd_i_renderer_get_current_cell_data(r);

    rd_foreach_operand(i, op, instr) {
        if(i > 0) rd_renderer_norm(r, p->operand_sep ? p->operand_sep : ", ");

        data->operand.index = i;
        data->operand.value = *op;

        bool done = false;

        if(p->render_operand)
            done = p->render_operand(r, instr, i, ctx->processor);

        if(!done) _rd_render_operand_default(r, instr, i, ctx->processor);
    }

    data->operand.value = (RDOperand){0};
    data->operand.index = -1;
}

const char* rd_get_reg_name(const RDContext* ctx, RDReg r) {
    if(ctx->processorplugin->get_reg_name)
        return ctx->processorplugin->get_reg_name(r, ctx->processor);

    return NULL;
}
