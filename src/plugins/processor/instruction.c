#include "core/context.h"
#include <redasm/plugins/processor/instruction.h>
#include <string.h>

bool rd_instruction_equals(const RDContext* ctx, const RDInstruction* instr,
                           const char* mnemonic) {
    if(!ctx->processorplugin->get_mnemonic || !instr || !mnemonic) return false;

    const char* m = ctx->processorplugin->get_mnemonic(instr, ctx->processor);
    if(!m) return false;
    return !strcmp(m, mnemonic);
}

void rd_instruction_set_mnemonic(RDInstruction* instr, const char* mnem) {
    if(!mnem) return;

    strncpy(instr->mnemonic, mnem, RD_MNEMONIC_LENGTH - 1);
    instr->mnemonic[RD_MNEMONIC_LENGTH - 1] = '\0';
}
