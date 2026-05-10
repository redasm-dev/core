#include "core/context.h"
#include <redasm/plugins/processor/instruction.h>
#include <string.h>

bool rd_instr_equals(const RDContext* ctx, const RDInstruction* self,
                     const char* mnemonic) {
    if(!ctx->processorplugin->get_mnemonic || !self || !mnemonic) return false;

    const char* m = ctx->processorplugin->get_mnemonic(self, ctx->processor);
    if(!m) return false;
    return !strcmp(m, mnemonic);
}

void rd_instr_set_mnemonic(RDInstruction* self, const char* mnem) {
    if(!mnem) return;

    strncpy(self->mnemonic, mnem, RD_MNEMONIC_LENGTH - 1);
    self->mnemonic[RD_MNEMONIC_LENGTH - 1] = '\0';
}
