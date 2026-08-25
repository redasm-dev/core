#pragma once

#include "core/function.h"
#include "core/registers.h"
#include "plugins/processor/processor.h"
#include <redasm/rdil/rdil.h>

typedef struct RDIL {
    RDContext* context;
    const RDFunction* function;
    RDRegisterHMap registers;
    RDInstructionVect lifted;
    RDAddress current_address;

    struct {
        RDAddress value;
        bool known;
    } target;

    bool done;
} RDIL;

const RDInstructionVect*
rd_i_il_lift_instruction(RDContext* ctx, const RDInstruction* real_instr,
                         RDInstructionVect* il);

const RDInstructionVect* rd_i_il_lift_prev(RDContext* ctx, RDAddress address,
                                           RDInstructionVect* il);

const RDInstructionVect* rd_i_il_lift(RDContext* ctx, RDAddress address,
                                      RDInstructionVect* il);
