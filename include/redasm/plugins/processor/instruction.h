#pragma once

#include <redasm/common.h>
#include <redasm/config.h>
#include <redasm/registers.h>

#define RD_MAX_OPERANDS 8
#define RD_MNEMONIC_LENGTH 32
#define RD_IS_DSLOT 0xFF

typedef enum {
    RD_OP_NULL = 0,
    RD_OP_CNST,
    RD_OP_REG,
    RD_OP_IMM,
    RD_OP_ADDR,  // Address immediate
    RD_OP_MEM,   // [Address]
    RD_OP_DISPL, // [BaseReg + IndexReg*Scale + Displ]
    RD_OP_SYM,

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
    RD_IF_RDIL, // only for RDIL instructions
} RDInstructionFlow;

typedef struct RDDisplOperand {
    RDReg base;
    RDReg index;
    i64 offset;
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
        u64 cnst;
        i64 s_cnst;
        RDReg reg;
        u64 addr;
        u64 imm;
        i64 s_imm;
        RDAddress mem;
        RDDisplOperand displ;
        RDUserOperand user;
        const char* sym;
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
        bool write_back;
        bool indirect;
    };

    union {
        void* userdata1;
        uptr uservalue1;
    };

    union {
        void* userdata2;
        uptr uservalue2;
        char mnemonic_buf[RD_MNEMONIC_LENGTH];
        const char* mnemonic;
    };
} RDInstruction;

typedef struct RDInstructionVect RDInstructionVect;

RD_API bool rd_instr_is_mnemonic(const RDInstruction* self,
                                 const char* mnemonic, const RDContext* ctx);

RD_API void rd_instr_set_mnemonic(RDInstruction* self, const char* mnem);

RD_API bool rd_instr_match(const RDContext* ctx, const RDInstruction* self,
                           const RDInstruction* shape);
RD_API bool rd_instr_match_n(const RDContext* ctx, const RDInstruction* instrs,
                             const RDInstruction* shapes, usize n);

static inline void rd_instr_set_op(RDInstruction* instr, int idx,
                                   const RDOperand* op) {
    instr->operands[idx] = *op;
}

static inline RDOperand* rd_instr_set_op_cnst(RDInstruction* instr, int idx,
                                              u64 c) {
    RDOperand* op = &instr->operands[idx];
    op->kind = RD_OP_CNST;
    op->cnst = c;
    return op;
}

static inline RDOperand* rd_instr_set_op_scnst(RDInstruction* instr, int idx,
                                               i64 c) {
    RDOperand* op = &instr->operands[idx];
    op->kind = RD_OP_CNST;
    op->s_cnst = c;
    return op;
}

static inline RDOperand* rd_instr_set_op_reg(RDInstruction* instr, int idx,
                                             RDReg id) {
    RDOperand* op = &instr->operands[idx];
    op->kind = RD_OP_REG;
    op->reg = id;
    return op;
}

static inline RDOperand* rd_instr_set_op_imm(RDInstruction* instr, int idx,
                                             u64 imm) {
    RDOperand* op = &instr->operands[idx];
    op->kind = RD_OP_IMM;
    op->imm = imm;
    return op;
}

static inline RDOperand* rd_instr_set_op_addr(RDInstruction* instr, int idx,
                                              RDAddress addr) {
    RDOperand* op = &instr->operands[idx];
    op->kind = RD_OP_ADDR;
    op->addr = addr;
    return op;
}

static inline RDOperand* rd_instr_set_op_mem(RDInstruction* instr, int idx,
                                             RDAddress mem) {
    RDOperand* op = &instr->operands[idx];
    op->kind = RD_OP_MEM;
    op->mem = mem;
    return op;
}

static inline RDOperand* rd_instr_set_op_displ(RDInstruction* instr, int idx,
                                               RDReg base, RDReg index,
                                               int scale, i64 offset) {
    RDOperand* op = &instr->operands[idx];
    op->kind = RD_OP_DISPL;
    op->displ.base = base;
    op->displ.index = index;
    op->displ.scale = scale;
    op->displ.offset = offset;
    return op;
}

static inline RDOperand* rd_instr_set_op_sym(RDInstruction* instr, int idx,
                                             const char* sym) {
    RDOperand* op = &instr->operands[idx];
    op->kind = RD_OP_SYM;
    op->sym = sym;
    return op;
}

static inline bool rd_instr_is_delay_slot(const RDInstruction* self) {
    return self->delay_slots == RD_IS_DSLOT;
}

static inline bool rd_instr_is_jump(const RDInstruction* self) {
    switch(self->flow) {
        case RD_IF_JUMP:
        case RD_IF_JUMP_COND: return true;
        default: break;
    }

    return false;
}

static inline bool rd_instr_is_call(const RDInstruction* self) {
    switch(self->flow) {
        case RD_IF_CALL:
        case RD_IF_CALL_COND: return true;
        default: break;
    }

    return false;
}

static inline bool rd_instr_is_branch(const RDInstruction* self) {
    return rd_instr_is_jump(self) || rd_instr_is_call(self);
}

static inline bool rd_instr_can_flow(const RDInstruction* self) {
    return self->flow != RD_IF_JUMP && self->flow != RD_IF_STOP;
}

static inline bool rd_instr_is_transparent(const RDInstruction* self) {
    return self->flow == RD_IF_NONE || self->flow == RD_IF_NOP;
}

static inline bool rd_instr_is_cond(const RDInstruction* self) {
    switch(self->flow) {
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
