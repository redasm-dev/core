#pragma once

#include <redasm/config.h>

#define RD_NOPERANDS 8
#define RD_NMNEMONIC 32

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
    RD_IF_STOP = (1 << 0),  // Stops flow
    RD_IF_JUMP = (1 << 1),  // Can branch
    RD_IF_CALL = (1 << 2),  // Call a function
    RD_IF_NOP = (1 << 3),   // No-Operation
    RD_IF_DSLOT = (1 << 4), // Delay Slot
} RDInstructionFeatures;

typedef struct RDPhraseOperand {
    int base;
    int index;
} RDPhraseOperand;

typedef struct RDDisplOperand {
    int base;
    int index;
    i64 displ;
    u8 scale;
} RDDisplOperand;

typedef struct RDUserOperand {
    union {
        int reg1;
        usize val1;
        isize s_val1;
    };

    union {
        int reg2;
        usize val2;
        isize s_val2;
    };
    union {
        int reg3;
        usize val3;
        isize s_val3;
    };
} RDUserOperand;

typedef struct RDOperand {
    u32 kind;
    u16 size;
    u16 count;

    union {
        int reg;
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
    u8 features;
    u8 delay_slots;
    RDOperand operands[RD_NOPERANDS];

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

#define _rd_foreach_op_arg(x) x

#define rd_foreach_operand(i, op, instr)                                       \
    for(int _rd_foreach_op_arg(i) = 0, _break_ = 1; _break_;                   \
        _break_ = !_break_)                                                    \
        for(const RDOperand* _rd_foreach_op_arg(op) = (instr)->operands;       \
            ((op) < (instr)->operands + RD_NOPERANDS) &&                       \
            ((op)->kind != RD_OP_NULL);                                        \
            (op)++, (i)++)
