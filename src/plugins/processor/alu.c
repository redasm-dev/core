#include "core/context.h"
#include "core/registers.h"
#include <inttypes.h>
#include <redasm/plugins/processor/alu.h>
#include <redasm/redasm.h>
#include <redasm/support/logging.h>

static const char* _rd_alu_slot_name(unsigned int slot) {
    static const char* const NAMES[] = {"dst", "src1", "src2"};
    assert(slot < rd_count_of(NAMES));
    return NAMES[slot];
}

static bool _rd_alu_validate_slot(const RDInstruction* instr,
                                  const RDAluOperand* op, unsigned int slot,
                                  bool allow_cnst, bool allow_none) {
    switch(op->kind) {
        case RD_ALU_NONE: {
            if(allow_none) return true;
            break;
        }

        case RD_ALU_INDEX: {
            if(op->index < RD_MAX_OPERANDS &&
               instr->operands[op->index].kind != RD_OP_NULL)
                return true;

            RD_LOG_FAIL("alu @ %" PRIx64 ": %s points to operand %zu, "
                        "which doesn't exist",
                        instr->address, _rd_alu_slot_name(slot), op->index);
            return false;
        }

        case RD_ALU_REGISTER: {
            if(op->reg != RD_REGID_INVALID) return true;
            break;
        }

        case RD_ALU_CONSTANT: {
            if(allow_cnst) return true;
            break;
        }

        default: break;
    }

    RD_LOG_FAIL("alu @ %" PRIx64 ": invalid %s kind %d", instr->address,
                _rd_alu_slot_name(slot), (int)op->kind);

    return false;
}

static bool _rd_alu_validate(const RDInstruction* instr, const RDAlu* alu) {
    if(alu->op == RD_ALUOP_INVALID || alu->op >= RD_ALUOP_COUNT) {
        RD_LOG_FAIL("alu @ %" PRIx64 ": invalid operation %d", instr->address,
                    (int)alu->op);
        return false;
    }

    bool unary = alu->op == RD_ALUOP_NOT;

    return _rd_alu_validate_slot(instr, &alu->dst, 0, false, false) &&
           _rd_alu_validate_slot(instr, &alu->src1, 1, true, false) &&
           _rd_alu_validate_slot(instr, &alu->src2, 2, !unary, unary);
}

static bool _rd_alu_slot_reg(const RDInstruction* instr, const RDAluOperand* op,
                             RDReg* reg) {
    if(op->kind == RD_ALU_REGISTER) {
        *reg = op->reg;
        return true;
    }

    if(op->kind == RD_ALU_INDEX &&
       instr->operands[op->index].kind == RD_OP_REG) {
        *reg = instr->operands[op->index].reg;
        return true;
    }

    return false;
}

static bool _rd_alu_resolve_dst(RDContext* ctx, const RDInstruction* instr,
                                const RDAluOperand* dst, RDRegResolved* res) {
    RDReg reg;
    return _rd_alu_slot_reg(instr, dst, &reg) &&
           rd_i_reg_resolve_id(ctx, reg, res);
}

static u8 _rd_alu_width(const RDRegResolved* res) {
    RDRegValue run = res->mask.mask >> res->mask.shift;
    u8 w = 0;

    while(run & 1) {
        run >>= 1;
        w++;
    }

    return w;
}

static bool _rd_alu_mem_address(RDContext* ctx, const RDOperand* op,
                                RDAddress* addr) {
    if(op->kind == RD_OP_MEM) {
        *addr = op->mem;
        return true;
    }

    if(op->kind != RD_OP_DISPL) return false;

    RDRegValue base = 0, index = 0;

    if(op->displ.base != RD_REGID_INVALID &&
       !rd_get_regval_id(ctx, op->displ.base, &base))
        return false;

    if(op->displ.index != RD_REGID_INVALID &&
       !rd_get_regval_id(ctx, op->displ.index, &index))
        return false;

    u8 scale = op->displ.scale ? op->displ.scale : 1;
    *addr = (RDAddress)(base + (index * scale) + (RDRegValue)op->displ.offset);
    return true;
}

static bool _rd_alu_read_const(RDContext* ctx, RDAddress addr, u8 width,
                               RDRegValue* v) {
    if(width % 8 || rd_is_writable(ctx, addr)) return false;
    return rd_i_read_uint(ctx, width / 8, addr, v);
}

static bool _rd_alu_read(RDContext* ctx, const RDInstruction* instr,
                         const RDAluOperand* src, u8 width, RDRegValue* v) {
    switch(src->kind) {
        case RD_ALU_CONSTANT: *v = (RDRegValue)src->cnst; return true;
        case RD_ALU_REGISTER: return rd_get_regval_id(ctx, src->reg, v);
        case RD_ALU_INDEX: break;
        default: return false;
    }

    assert(src->index < RD_MAX_OPERANDS);
    const RDOperand* op = &instr->operands[src->index];

    switch(op->kind) {
        case RD_OP_REG: return rd_get_regval_id(ctx, op->reg, v);
        case RD_OP_CNST: *v = op->cnst; break;
        case RD_OP_IMM: *v = op->imm; break;

        case RD_OP_MEM:
        case RD_OP_DISPL: {
            RDAddress addr;
            return _rd_alu_mem_address(ctx, op, &addr) &&
                   _rd_alu_read_const(ctx, addr, width, v);
        }

        default: return false; // ADDR, SYM, STUB, custom kinds
    }

    return true;
}

// 'xor r, r' / 'sub r, r' are 0 whatever r holds
static bool _rd_alu_identity(const RDInstruction* instr, const RDAlu* alu,
                             RDRegValue* res) {
    if(alu->op != RD_ALUOP_XOR && alu->op != RD_ALUOP_SUB) return false;

    RDReg r1, r2;
    if(!_rd_alu_slot_reg(instr, &alu->src1, &r1) ||
       !_rd_alu_slot_reg(instr, &alu->src2, &r2) || r1 != r2)
        return false;

    *res = 0;
    return true;
}
static bool _rd_alu_compute(RDAluOp op, u8 width, RDRegValue a, RDRegValue b,
                            RDRegValue* res) {
    RDRegValue m = rd_i_reg_get_mask(width, 0);
    a &= m;

    switch(op) {
        case RD_ALUOP_ADD: *res = a + b; break;
        case RD_ALUOP_SUB: *res = a - b; break;
        case RD_ALUOP_MUL: *res = a * b; break;

        case RD_ALUOP_DIV:
            if(!(b & m)) return false;
            *res = a / (b & m);
            break;

        case RD_ALUOP_MOD:
            if(!(b & m)) return false;
            *res = a % (b & m);
            break;

        case RD_ALUOP_AND: *res = a & b; break;
        case RD_ALUOP_OR: *res = a | b; break;
        case RD_ALUOP_XOR: *res = a ^ b; break;

        case RD_ALUOP_LSL:
        case RD_ALUOP_ASL: *res = b >= width ? 0 : a << b; break;
        case RD_ALUOP_LSR: *res = b >= width ? 0 : a >> b; break;

        case RD_ALUOP_ASR: {
            // sign-extend from 'width', shifting past it fills with the sign
            RDRegValue sign = (RDRegValue)1 << (width - 1);
            i64 x = (i64)((a ^ sign) - sign);
            unsigned int s =
                b >= width ? (unsigned int)width - 1 : (unsigned int)b;
            *res = (RDRegValue)(x >> s);
            break;
        }

        case RD_ALUOP_ROL:
        case RD_ALUOP_ROR: {
            unsigned int s = (unsigned int)(b % width);
            if(op == RD_ALUOP_ROR && s)
                s = width - s; // ror n == rol (width - n)

            *res = s ? (a << s) | (a >> (width - s)) : a;
            break;
        }

        case RD_ALUOP_NOT: *res = ~a; break;
        default: return false;
    }

    *res &= m;
    return true;
}

RDAluStatus rd_alu(RDContext* ctx, const RDInstruction* instr,
                   const RDAlu* alu) {
    if(!ctx || !instr || !alu || !_rd_alu_validate(instr, alu))
        return RD_ALU_SKIP;

    RDRegResolved dst;
    if(!_rd_alu_resolve_dst(ctx, instr, &alu->dst, &dst)) return RD_ALU_SKIP;

    RDRegisterHMap* regs = &ctx->engine.current.registers;
    u8 width = _rd_alu_width(&dst);
    RDRegValue a, b = 0, res;

    bool known = _rd_alu_identity(instr, alu, &res) ||
                 (_rd_alu_read(ctx, instr, &alu->src1, width, &a) &&
                  (alu->op == RD_ALUOP_NOT ||
                   _rd_alu_read(ctx, instr, &alu->src2, width, &b)) &&
                  _rd_alu_compute(alu->op, width, a, b, &res));

    if(!known) {
        rd_i_regmap_del(regs, &dst); // dst changed to something we can't tell
        return RD_ALU_UNKNOWN;
    }

    rd_i_regmap_set(regs, &dst, res, rd_i_current_setter(ctx));
    return RD_ALU_DONE;
}
