#pragma once

#include <redasm/plugins/processor/processor.h>

typedef struct RDInstructionVect {
    RDInstruction* data;
    usize length;
    usize capacity;

    RDInstruction real_instr;
} RDInstructionVect;

bool rd_i_processor_render_operand(RDRenderer* r, const RDInstruction* instr,
                                   int idx, RDProcessor* p);
void rd_i_processor_render_instruction(RDRenderer* r,
                                       const RDInstruction* instr);
