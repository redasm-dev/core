#pragma once

// clang-format off

/*
 * *** RDIL - REDasm Intermediate Language ***
 *
 * RDIL lifts host instructions to architecture-neutral RDInstruction structs.
 * One host instruction may produce multiple RDIL instructions.
 * RDIL instructions are generated on demand via rd_lift() and never persisted.
 *
 * RDILStatement: statement opcodes, stored in RDInstruction.id
 *
 * Statement operand rules:
 *   - ALU dst (op[0]) must always be REG or SYM
 *   - MOV dst (op[0]) can be REG, MEM, or SYM
 *   - POP dst (op[0]) can be REG or SYM
 *   - GOTO/CALL target (op[0]) can be any expression
 *   - GOTO_<cond>/CALL_<cond> : op[0]=lhs, op[1]=rhs, op[2]=target
 *
 * +-----------+------------+-----------+-----------+----------------------------------+
 * | Statement | Operand 1  | Operand 2 | Operand 3 |           Description            |
 * +-----------+------------+-----------+-----------+----------------------------------+
 * | Unkn      |            |           |           | unknown/unlifted instruction     |
 * | Nop       |            |           |           | no operation                     |
 * | Trap      |            |           |           | software interrupt/syscall       |
 * | Ret       |            |           |           | return from function             |
 * | Mov       | dst        | src       |           | dst = src                        |
 * | Push      | src        |           |           | push src onto stack              |
 * | Pop       | dst        |           |           | dst = pop from stack             |
 * | Jump      | dst        |           |           | goto dst                         |
 * | Jump.eq   | lhs        | rhs       | dst       | if (lhs == rhs) goto dst         |
 * | Jump.ne   | lhs        | rhs       | dst       | if (lhs != rhs) goto dst         |
 * | Jump.lt   | lhs        | rhs       | dst       | if (lhs < rhs) goto dst          |
 * | Jump.le   | lhs        | rhs       | dst       | if (lhs <= rhs) goto dst         |
 * | Jump.gt   | lhs        | rhs       | dst       | if (lhs > rhs) goto dst          |
 * | Jump.ge   | lhs        | rhs       | dst       | if (lhs >= rhs) goto dst         |
 * | Call      | dst        |           |           | call dst                         |
 * | Call.eq   | lhs        | rhs       | dst       | if (lhs == rhs) call dst         |
 * | Call.ne   | lhs        | rhs       | dst       | if (lhs != rhs) call dst         |
 * | Call.lt   | lhs        | rhs       | dst       | if (lhs < rhs) call dst          |
 * | Call.le   | lhs        | rhs       | dst       | if (lhs <= rhs) call dst         |
 * | Call.gt   | lhs        | rhs       | dst       | if (lhs > rhs) call dst          |
 * | Call.ge   | lhs        | rhs       | dst       | if (lhs >= rhs) call dst         |
 * | Add       | dst[reg]   | src1      | src2      | dst = src1 + src2                |
 * | Sub       | dst[reg]   | src1      | src2      | dst = src1 - src2                |
 * | Mul       | dst[reg]   | src1      | src2      | dst = src1 * src2                |
 * | Div       | dst[reg]   | src1      | src2      | dst = src1 / src2                |
 * | Mod       | dst[reg]   | src1      | src2      | dst = src1 % src2                |
 * | And       | dst[reg]   | src1      | src2      | dst = src1 & src2                |
 * | Or        | dst[reg]   | src1      | src2      | dst = src1 | src2                |
 * | Xor       | dst[reg]   | src1      | src2      | dst = src1 ^ src2                |
 * | Lsl       | dst[reg]   | src1      | src2      | dst = src1 << src2               |
 * | Lsr       | dst[reg]   | src1      | src2      | dst = src1 >> src2 (logical)     |
 * | Asl       | dst[reg]   | src1      | src2      | dst = src1 <<< src2 (arithmetic) |
 * | Asr       | dst[reg]   | src1      | src2      | dst = src1 >>> src2 (arithmetic) |
 * | Rol       | dst[reg]   | src1      | src2      | dst = rol(src1, src2)            |
 * | Ror       | dst[reg]   | src1      | src2      | dst = ror(src1, src2)            |
 * | Not       | dst[reg]   | src       |           | dst = ~src                       |
 * +-----------+------------+-----------+-----------+----------------------------------+
 */

// clang-format on

#include <redasm/config.h>
#include <redasm/registers.h>

// clang-format off
typedef enum {
    RD_IL_UNKNOWN = 0, 
    RD_IL_NOP, RD_IL_TRAP, RD_IL_RET, RD_IL_MOV, 

    RD_IL_PUSH, RD_IL_POP,

    RD_IL_JUMP,
    RD_IL_JUMP_EQ, RD_IL_JUMP_NE,
    RD_IL_JUMP_LT, RD_IL_JUMP_LE,
    RD_IL_JUMP_GT, RD_IL_JUMP_GE,

    RD_IL_CALL, 
    RD_IL_CALL_EQ, RD_IL_CALL_NE,
    RD_IL_CALL_LT, RD_IL_CALL_LE,
    RD_IL_CALL_GT, RD_IL_CALL_GE,

    RD_IL_ADD, RD_IL_SUB, RD_IL_MUL, RD_IL_DIV, RD_IL_MOD,
    RD_IL_AND, RD_IL_OR, RD_IL_XOR,
    RD_IL_LSL, RD_IL_LSR,
    RD_IL_ASL, RD_IL_ASR,
    RD_IL_ROL, RD_IL_ROR,
    RD_IL_NOT,

    RD_IL_COUNT,
} RDILStatement;
// clang-format on
