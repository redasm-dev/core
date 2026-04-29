#include "plugins/processor/processor.h"
#include <redasm/plugins/processor/instruction.h>
#include <string.h>

bool rd_instruction_equals(const RDContext* ctx, const RDInstruction* instr,
                           const char* mnemonic) {
    if(!instr || !mnemonic) return false;

    const char* m = rd_i_processor_get_mnemonic(ctx, instr);
    if(!m) return false;
    return !strcmp(m, mnemonic);
}
