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
