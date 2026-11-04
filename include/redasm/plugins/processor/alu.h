#pragma once

#include <redasm/common.h>
#include <redasm/plugins/processor/instruction.h>
#include <redasm/registers.h>

typedef enum {
    RD_ALUOP_INVALID = 0,
    RD_ALUOP_ADD,
    RD_ALUOP_SUB,
    RD_ALUOP_MUL,
    RD_ALUOP_DIV,
    RD_ALUOP_MOD,
    RD_ALUOP_AND,
    RD_ALUOP_OR,
    RD_ALUOP_XOR,
    RD_ALUOP_LSL,
    RD_ALUOP_LSR,
    RD_ALUOP_ASL,
    RD_ALUOP_ASR,
    RD_ALUOP_ROL,
    RD_ALUOP_ROR,
    RD_ALUOP_NOT,

    RD_ALUOP_COUNT,
} RDAluOp;

typedef enum {
    RD_ALU_NONE = 0,
    RD_ALU_INDEX,
    RD_ALU_REGISTER,
    RD_ALU_CONSTANT,
} RDAluOperandKind;

typedef struct RDAluOperand {
    RDAluOperandKind kind;

    union {
        usize index;
        RDReg reg;
        i64 cnst;
    };
} RDAluOperand;

typedef enum {
    RD_ALU_DONE,
    RD_ALU_UNKNOWN,
    RD_ALU_SKIP,
} RDAluStatus;

typedef struct RDAlu {
    RDAluOp op;
    RDAluOperand dst;
    RDAluOperand src1, src2;
} RDAlu;

RD_API RDAluStatus rd_alu(RDContext* ctx, const RDInstruction* instr,
                          const RDAlu* alu);
