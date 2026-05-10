#pragma once

#include <redasm/rdil/opcodes.h>

typedef struct RDILStatementInfo {
    const char* mnemonic;
    int n_operands;
} RDILStatementInfo;

extern const RDILStatementInfo RDIL_OP_TABLE[RD_IL_COUNT];
