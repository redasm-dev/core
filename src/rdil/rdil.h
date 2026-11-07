#pragma once

#include "core/registers.h"
#include "plugins/processor/processor.h"
#include <redasm/rdil/rdil.h>

typedef struct RDIL {
    RDContext* context;
    RDRegisterVect registers;
    RDInstructionVect lifted;
    RDRelAddress current_address;

    struct {
        RDRelAddress value;
        bool has_value;
    } entry;

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
