#include "core/context.h"
#include <redasm/plugins/processor/instruction.h>
#include <string.h>

bool rd_instr_is_mnemonic(const RDInstruction* self, const char* mnemonic,
                          const RDContext* ctx) {
    if(!ctx->processorplugin->get_mnemonic || !self || !mnemonic) return false;

    const char* m = ctx->processorplugin->get_mnemonic(self, ctx->processor);
    if(!m) return false;
    return !strcmp(m, mnemonic);
}

void rd_instr_set_mnemonic(RDInstruction* self, const char* mnem) {
    if(!mnem) return;

    strncpy(self->mnemonic_buf, mnem, RD_MNEMONIC_LENGTH - 1);
    self->mnemonic_buf[RD_MNEMONIC_LENGTH - 1] = '\0';
}

bool rd_instr_match(const RDContext* ctx, const RDInstruction* self,
                    const RDInstruction* shape) {
    if(shape->mnemonic) {
        const char* mnem = NULL;

        if(self->flow != RD_IF_RDIL) {
            const RDProcessorPlugin* plugin = ctx->processorplugin;
            if(!plugin->get_mnemonic) return false;
            mnem = plugin->get_mnemonic(self, ctx->processor);
        }
        else
            mnem = self->mnemonic;

        if(!mnem || rd_stricmp(mnem, shape->mnemonic) != 0) return false;
    }
    else if(self->id != shape->id)
        return false;

    if(self->delay_slots != shape->delay_slots) return false;
    if(self->indirect != shape->indirect) return false;

    rd_foreach_operand(i, op, self) {
        if(op->kind != shape->operands[i].kind) return false;
    }

    return true;
}

bool rd_instr_match_n(const RDContext* ctx, const RDInstruction* instrs,
                      const RDInstruction* shapes, usize n) {
    for(usize i = 0; i < n; i++) {
        if(!rd_instr_match(ctx, instrs + i, shapes + i)) return false;
    }

    return true;
}
