#include "opcodes.h"
#include <redasm/rdil/opcodes.h>

const RDILStatementInfo RDIL_OP_TABLE[RD_IL_COUNT] = {
    [RD_IL_UNKNOWN] = {"unknown", 0},

    [RD_IL_NOP] = {"nop", 0},         [RD_IL_TRAP] = {"trap", 0},
    [RD_IL_RET] = {"ret", 0},         [RD_IL_MOV] = {"mov", 2},

    [RD_IL_PUSH] = {"push", 1},       [RD_IL_POP] = {"pop", 1},

    [RD_IL_JUMP] = {"jump", 1},       [RD_IL_JUMP_EQ] = {"jump.eq", 3},
    [RD_IL_JUMP_NE] = {"jump.ne", 3}, [RD_IL_JUMP_LT] = {"jump.lt", 3},
    [RD_IL_JUMP_LE] = {"jump.le", 3}, [RD_IL_JUMP_GT] = {"jump.gt", 3},
    [RD_IL_JUMP_GE] = {"jump.ge", 3},

    [RD_IL_CALL] = {"call", 1},       [RD_IL_CALL_EQ] = {"call.eq", 3},
    [RD_IL_CALL_NE] = {"call.ne", 3}, [RD_IL_CALL_LT] = {"call.lt", 3},
    [RD_IL_CALL_LE] = {"call.le", 3}, [RD_IL_CALL_GT] = {"call.gt", 3},
    [RD_IL_CALL_GE] = {"call.ge", 3},

    [RD_IL_ADD] = {"add", 3},         [RD_IL_SUB] = {"sub", 3},
    [RD_IL_MUL] = {"mul", 3},         [RD_IL_DIV] = {"div", 3},
    [RD_IL_MOD] = {"mod", 3},         [RD_IL_AND] = {"and", 3},
    [RD_IL_OR] = {"or", 3},           [RD_IL_XOR] = {"xor", 3},
    [RD_IL_LSL] = {"lsl", 3},         [RD_IL_LSR] = {"lsr", 3},
    [RD_IL_ASL] = {"asl", 3},         [RD_IL_ASR] = {"asr", 3},
    [RD_IL_ROL] = {"rol", 3},         [RD_IL_ROR] = {"ror", 3},
    [RD_IL_NOT] = {"not", 3},
};
