#pragma once

#include <redasm/common.h>
#include <redasm/config.h>
#include <redasm/registers.h>

#define RD_MAX_OPERANDS 8
#define RD_NMNEMONIC 32
#define RD_IS_DSLOT 0xFF

typedef enum {
    RD_OP_NULL = 0,
    RD_OP_REG,
    RD_OP_IMM,
    RD_OP_ADDR,   // Address immediate
    RD_OP_MEM,    // [Address]
    RD_OP_PHRASE, // [BaseReg + IndexReg]
    RD_OP_DISPL,  // [BaseReg + IndexReg + Displ]

    RD_OP_USERBASE, // User defined operands
} RDOperandKind;

typedef enum {
    RD_IF_NONE = 0,
    RD_IF_JUMP,
    RD_IF_JUMP_COND,
    RD_IF_CALL,
    RD_IF_CALL_COND,
    RD_IF_STOP,
    RD_IF_NOP,
} RDInstructionFlow;

typedef struct RDPhraseOperand {
    RDReg base;
    RDReg index;
} RDPhraseOperand;

typedef struct RDDisplOperand {
    RDReg base;
    RDReg index;
    i64 displ;
    u8 scale;
} RDDisplOperand;

typedef struct RDUserOperand {
    union {
        RDReg reg1;
        usize val1;
        isize s_val1;
    };

    union {
        RDReg reg2;
        usize val2;
        isize s_val2;
    };
    union {
        RDReg reg3;
        usize val3;
        isize s_val3;
    };
} RDUserOperand;

typedef struct RDOperand {
    RDOperandKind kind;
    u16 size;
    u16 count;

    union {
        RDReg reg;
        u64 addr;
        u64 imm;
        i64 s_imm;
        RDAddress mem;
        RDPhraseOperand phrase;
        RDDisplOperand displ;
        RDUserOperand user;
    };

    u32 userdata1;
    u32 userdata2;
} RDOperand;

typedef struct RDInstruction {
    RDAddress address;
    u32 id;
    u16 length;
    u8 flow;
    u8 delay_slots;
    RDOperand operands[RD_MAX_OPERANDS];

    union {
        void* userdata1;
        uptr uservalue1;
    };

    union {
        void* userdata2;
        uptr uservalue2;
        char mnemonic[RD_NMNEMONIC];
    };
} RDInstruction;

RD_API bool rd_instruction_equals(const RDContext* ctx,
                                  const RDInstruction* instr,
                                  const char* mnemonic);

static inline bool rd_is_delay_slot(const RDInstruction* instr) {
    return instr->delay_slots == RD_IS_DSLOT;
}

static inline bool rd_is_jump(const RDInstruction* instr) {
    switch(instr->flow) {
        case RD_IF_JUMP:
        case RD_IF_JUMP_COND: return true;
        default: break;
    }

    return false;
}

static inline bool rd_is_call(const RDInstruction* instr) {
    switch(instr->flow) {
        case RD_IF_CALL:
        case RD_IF_CALL_COND: return true;
        default: break;
    }

    return false;
}

static inline bool rd_is_branch(const RDInstruction* instr) {
    return rd_is_jump(instr) || rd_is_call(instr);
}

static inline bool rd_can_flow(const RDInstruction* instr) {
    return instr->flow != RD_IF_JUMP && instr->flow != RD_IF_STOP;
}

static inline bool rd_is_cond(const RDInstruction* instr) {
    switch(instr->flow) {
        case RD_IF_JUMP_COND:
        case RD_IF_CALL_COND: return true;
        default: break;
    }

    return false;
}

#define _rd_foreach_op_arg(x) x

#define rd_foreach_operand(i, op, instr)                                       \
    for(int _rd_foreach_op_arg(i) = 0, _break_ = 1; _break_;                   \
        _break_ = !_break_)                                                    \
        for(const RDOperand* _rd_foreach_op_arg(op) = (instr)->operands;       \
            ((op) < (instr)->operands + RD_MAX_OPERANDS) &&                    \
            ((op)->kind != RD_OP_NULL);                                        \
            (op)++, (i)++)
