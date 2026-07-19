#include "processor.h"
#include "core/context.h"
#include "core/state.h"
#include "plugins/common.h"
#include "surface/renderer.h"
#include <redasm/support/logging.h>
#include <stddef.h>

bool rd_i_processor_render_operand(RDRenderer* r, const RDInstruction* instr,
                                   int idx, RDProcessor* p) {
    RD_UNUSED(p);
    const RDProcessorPlugin* plugin = r->context->processorplugin;
    const RDOperand* op = &instr->operands[idx];
    const unsigned int ADDR_W = plugin->ptr_size * 2;

    switch(op->kind) {
        case RD_OP_CNST:
            rd_renderer_num(r, op->s_imm, 16, 0, RD_NUM_NOADDR);
            break;

        case RD_OP_REG: rd_renderer_reg(r, op->reg); break;

        case RD_OP_IMM:
            rd_renderer_num(r, op->s_imm, 16, 0, RD_NUM_DEFAULT);
            break;

        case RD_OP_ADDR:
            rd_renderer_loc(r, op->addr, ADDR_W, RD_NUM_DEFAULT);
            break;

        case RD_OP_MEM:
            rd_renderer_norm(r, "[");
            rd_renderer_loc(r, op->mem, ADDR_W, RD_NUM_DEFAULT);
            rd_renderer_norm(r, "]");
            break;

        case RD_OP_DISPL: {
            rd_renderer_norm(r, "[");
            rd_renderer_reg(r, op->displ.base);

            if(op->displ.index != RD_REGID_UNKNOWN) {
                rd_renderer_norm(r, "+");
                rd_renderer_reg(r, op->displ.index);

                if(op->displ.scale > 1) {
                    rd_renderer_norm(r, "*");
                    rd_renderer_num(r, op->displ.scale, 16, 0, RD_NUM_DEFAULT);
                }
            }

            if(op->displ.offset > 0)
                rd_renderer_norm(r, "+");
            else if(op->displ.offset < 0)
                rd_renderer_norm(r, "-");

            if(op->displ.offset != 0)
                rd_renderer_num(r, op->displ.offset, 16, 0, RD_NUM_DEFAULT);

            rd_renderer_norm(r, "]");
            break;
        }

        case RD_OP_SYM: {
            rd_renderer_text(r, op->sym ? op->sym : "???", RD_THEME_LOCATION,
                             RD_THEME_BACKGROUND);
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

        done = false;

        if(p->render_operand)
            done = p->render_operand(r, instr, i, ctx->processor);

        if(!done) rd_i_processor_render_operand(r, instr, i, ctx->processor);
    }

    data->operand.value = (RDOperand){0};
    data->operand.index = -1;
}

const char* rd_get_reg_name(const RDContext* ctx, RDReg r) {
    if(ctx->processorplugin->get_reg_name)
        return ctx->processorplugin->get_reg_name(r, ctx->processor);

    return NULL;
}

unsigned int rd_get_ptr_size(const RDContext* ctx) {
    assert(ctx->processorplugin);
    return ctx->processorplugin->ptr_size;
}

unsigned int rd_get_code_ptr_size(const RDContext* ctx) {
    assert(ctx->processorplugin);
    return ctx->processorplugin->code_ptr_size
               ? ctx->processorplugin->code_ptr_size
               : ctx->processorplugin->ptr_size;
}

unsigned int rd_get_int_size(const RDContext* ctx) {
    assert(ctx->processorplugin);
    return ctx->processorplugin->int_size;
}

bool rd_register_processor(const RDProcessorPlugin* p) {
    if(!rd_i_validate_plugin_with_name(p->level, p->id, p->name, "processor"))
        return false;

    if(!p->decode) {
        RD_LOG_FAIL("processor '%s' requires a decoder", p->id);
        return false;
    }

    if(!p->emulate) {
        RD_LOG_FAIL("processor '%s' requires an emulator", p->id);
        return false;
    }

    if(!p->ptr_size) {
        RD_LOG_FAIL("invalid pointer-size for processor '%s'", p->id);
        return false;
    }

    if(!p->int_size) {
        RD_LOG_FAIL("invalid integer-size for processor '%s'", p->id);
        return false;
    }

    if(rd_processor_find(p->id)) {
        RD_LOG_WARN("processor '%s' already registered", p->id);
        return false;
    }

    RD_LOG_DEBUG("registering processor '%s' [%s]", p->id, p->name);
    RDPlugin* plugin = rd_alloc(sizeof(*plugin));
    plugin->processor = p;
    vect_push(&rd_i_state.processors, plugin);
    return true;
}
