#pragma once

// clang-format off
/*
 * *** RDIL Opcode Table ***
 *
 * Encoding: prefix bytecode, self-delimiting (arity is fixed per opcode)
 * Leaves use ULEB128/SLEB128 for compact number encoding
 *
 * Statement-level opcodes (valid at top level):
 *   NOP, UNKNOWN, COPY, GOTO, CALL, RET, IF, PUSH, POP, TRAP
 *
 * Expression-level opcodes (valid as operands only):
 *   REG, VAR, SYM, UINT, SINT, MEM, NOT,
 *   ADD, SUB, MUL, DIV, MOD,
 *   AND, OR, XOR,
 *   LSL, LSR, ASL, ASR,
 *   ROL, ROR,
 *   EQ, NE, LT, LE, GT, GE
 *
 *   +---------+------------+-----------+-----------+----------------------------------+
 *   | Opcode  | Operand 1  | Operand 2 | Operand 3 |           Description            |
 *   +---------+------------+-----------+-----------+----------------------------------+
 *   | Nop     |            |           |           | nop                              |
 *   | Unknown |            |           |           | unknown                          |
 *   | Copy    | dst        | src       |           | dst = src                        |
 *   | Goto    | target     |           |           | goto target                      |
 *   | Call    | target     |           |           | target()                         |
 *   | Ret     | target     |           |           | ret target                       |
 *   | If      | cond       | t         | f         | if (cond) goto t else goto f     |
 *   | Push    | src        |           |           | push (src)                       |
 *   | Pop     | dst        |           |           | dst = pop()                      |
 *   | Trap    | u          |           |           | trap u                           |
 *   +---------+------------+-----------+-----------+----------------------------------+
 *   | Reg     | [uleb]     |           |           | register id                      |
 *   | Var     | [uleb]     |           |           | address                          |
 *   | Sym     | [bytes][0] |           |           | named abstract slot              |
 *   | UInt    | [uleb]     |           |           | unsigned constant                |
 *   | SInt    | [sleb]     |           |           | signed constant                  |
 *   +---------+------------+-----------+-----------+----------------------------------+
 *   | Mem     | u          |           |           | [u]                              |
 *   | Not     | u          |           |           | ~u                               |
 *   | Add     | l          | r         |           | l + r                            |
 *   | Sub     | l          | r         |           | l - r                            |
 *   | Mul     | l          | r         |           | l * r                            |
 *   | Div     | l          | r         |           | l / r                            |
 *   | Mod     | l          | r         |           | l % r                            |
 *   | And     | l          | r         |           | l & r                            |
 *   | Or      | l          | r         |           | l | r                            |
 *   | Xor     | l          | r         |           | l ^ r                            |
 *   | Lsl     | l          | r         |           | l << r                           |
 *   | Lsr     | l          | r         |           | l >> r                           |
 *   | Asl     | l          | r         |           | l <<< r                          |
 *   | Asr     | l          | r         |           | l >>> r                          |
 *   | Rol     | l          | r         |           | rotl(l, r)                       |
 *   | Ror     | l          | r         |           | rotr(l, r)                       |
 *   | Eq      | l          | r         |           | l == r                           |
 *   | Ne      | l          | r         |           | l != r                           |
 *   | Lt      | l          | r         |           | l < r                            |
 *   | Le      | l          | r         |           | l <= r                           |
 *   | Gt      | l          | r         |           | l > r                            |
 *   | Ge      | l          | r         |           | l >= r                           |
 *   +---------+------------+-----------+-----------+----------------------------------+
 */
// clang-format on

#include <redasm/config.h>

typedef struct RDILInstruction RDILInstruction;

// clang-format off
typedef enum RDILOp {
    // Statement-level
    RD_IL_NOP = 0, RD_IL_UNKN, RD_IL_COPY, RD_IL_GOTO, RD_IL_CALL, RD_IL_RET, RD_IL_IF, RD_IL_PUSH, RD_IL_POP, RD_IL_TRAP,

    // Leaves
    RD_IL_REG, RD_IL_VAR, RD_IL_SYM, RD_IL_UINT, RD_IL_SINT,

    // Unary expressions
    RD_IL_MEM, RD_IL_NOT,

    // Binary expressions
    RD_IL_ADD, RD_IL_SUB, RD_IL_MUL, RD_IL_DIV, RD_IL_MOD,
    RD_IL_AND, RD_IL_OR, RD_IL_XOR,
    RD_IL_LSL, RD_IL_LSR,
    RD_IL_ASL, RD_IL_ASR,
    RD_IL_ROL, RD_IL_ROR,
    RD_IL_EQ, RD_IL_NE,
    RD_IL_LT, RD_IL_LE,
    RD_IL_GT, RD_IL_GE,
} RDILOp;
// clang-format on

// ---------------------------------------------------------------------------
// statements
// ---------------------------------------------------------------------------

RD_API void rd_il_nop(RDILInstruction* self);
RD_API void rd_il_unkn(RDILInstruction* self);
RD_API void rd_il_copy(RDILInstruction* self);
RD_API void rd_il_goto(RDILInstruction* self);
RD_API void rd_il_call(RDILInstruction* self);
RD_API void rd_il_ret(RDILInstruction* self);
RD_API void rd_il_if(RDILInstruction* self);
RD_API void rd_il_push(RDILInstruction* self);
RD_API void rd_il_pop(RDILInstruction* self);
RD_API void rd_il_trap(RDILInstruction* self);

// ---------------------------------------------------------------------------
// leaves
// ---------------------------------------------------------------------------

RD_API void rd_il_reg(RDILInstruction* self, int reg);
RD_API void rd_il_var(RDILInstruction* self, RDAddress address);
RD_API void rd_il_sym(RDILInstruction* self, const char* name);
RD_API void rd_il_uint(RDILInstruction* self, u64 value);
RD_API void rd_il_sint(RDILInstruction* self, i64 value);

// ---------------------------------------------------------------------------
// unary expressions
// ---------------------------------------------------------------------------

RD_API void rd_il_mem(RDILInstruction* self);
RD_API void rd_il_not(RDILInstruction* self);

// ---------------------------------------------------------------------------
// binary expressions
// ---------------------------------------------------------------------------

RD_API void rd_il_add(RDILInstruction* self);
RD_API void rd_il_sub(RDILInstruction* self);
RD_API void rd_il_mul(RDILInstruction* self);
RD_API void rd_il_div(RDILInstruction* self);
RD_API void rd_il_mod(RDILInstruction* self);
RD_API void rd_il_and(RDILInstruction* self);
RD_API void rd_il_or(RDILInstruction* self);
RD_API void rd_il_xor(RDILInstruction* self);
RD_API void rd_il_lsl(RDILInstruction* self);
RD_API void rd_il_lsr(RDILInstruction* self);
RD_API void rd_il_asl(RDILInstruction* self);
RD_API void rd_il_asr(RDILInstruction* self);
RD_API void rd_il_rol(RDILInstruction* self);
RD_API void rd_il_ror(RDILInstruction* self);
RD_API void rd_il_eq(RDILInstruction* self);
RD_API void rd_il_ne(RDILInstruction* self);
RD_API void rd_il_lt(RDILInstruction* self);
RD_API void rd_il_le(RDILInstruction* self);
RD_API void rd_il_gt(RDILInstruction* self);
RD_API void rd_il_ge(RDILInstruction* self);
