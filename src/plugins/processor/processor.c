#include "processor.h"
#include "core/context.h"
#include "surface/renderer.h"

void rd_i_processor_render_instruction(RDRenderer* r,
                                       const RDInstruction* instr) {
    const RDContext* ctx = r->context;
    const RDProcessorPlugin* p = ctx->processorplugin;

    if(p->render_mnemonic) p->render_mnemonic(r, instr, ctx->processor);
    rd_renderer_ws(r, 1);

    if(p->render_operand) {
        rd_foreach_operand(i, op, instr) {
            if(i > 0) {
                rd_renderer_norm(r, p->operand_sep ? p->operand_sep : ", ");
            }

            p->render_operand(r, instr, i, ctx->processor);
        }
    }
}

const char* rd_get_register_name(const RDContext* ctx, RDReg r) {
    if(ctx->processorplugin->get_register_name)
        return ctx->processorplugin->get_register_name(r, ctx->processor);

    return NULL;
}

RDReg rd_get_register_id(const RDContext* ctx, const char* name) {
    if(name && ctx->processorplugin->get_register_id)
        return ctx->processorplugin->get_register_id(name, ctx->processor);

    return RD_REGID_UNKNOWN;
}
