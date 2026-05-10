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

const RDInstructionVect* rd_il_lift(RDContext* ctx, RDAddress address,
                                    RDInstructionVect* il);
