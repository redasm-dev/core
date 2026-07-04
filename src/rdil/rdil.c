#include "rdil.h"
#include "core/context.h"
#include "db/db.h"
#include "rdil/opcodes.h"
#include "support/error.h"
#include <redasm/support/logging.h>

typedef struct RDILValue {
    u64 value;
    bool known;
} RDILValue;

static bool _rd_il_eval_cond(const RDInstruction* il, const RDILValue* lhs,
                             const RDILValue* rhs) {
    assert(lhs->known && rhs->known);

    switch(il->id) {
        case RD_IL_JUMP_EQ:
        case RD_IL_CALL_EQ: return lhs->value == rhs->value;

        case RD_IL_JUMP_NE:
        case RD_IL_CALL_NE: return lhs->value != rhs->value;

        case RD_IL_JUMP_LT:
        case RD_IL_CALL_LT: return lhs->value < rhs->value;

        case RD_IL_JUMP_LE:
        case RD_IL_CALL_LE: return lhs->value <= rhs->value;

        case RD_IL_JUMP_GT:
        case RD_IL_CALL_GT: return lhs->value > rhs->value;

        case RD_IL_JUMP_GE:
        case RD_IL_CALL_GE: return lhs->value >= rhs->value;

        default: break;
    }

    unreachable();
    return false;
}

static RDILValue _rd_il_eval_alu(const RDInstruction* il, const RDILValue* lhs,
                                 const RDILValue* rhs) {
    if(!lhs->known || !rhs->known) return (RDILValue){0};

    u64 val = 0;

    switch(il->id) {
        case RD_IL_ADD: val = lhs->value + rhs->value; break;
        case RD_IL_SUB: val = lhs->value - rhs->value; break;
        case RD_IL_MUL: val = lhs->value * rhs->value; break;
        case RD_IL_DIV: val = rhs->value ? lhs->value / rhs->value : 0; break;
        case RD_IL_MOD: val = rhs->value ? lhs->value % rhs->value : 0; break;
        case RD_IL_AND: val = lhs->value & rhs->value; break;
        case RD_IL_OR: val = lhs->value | rhs->value; break;
        case RD_IL_XOR: val = lhs->value ^ rhs->value; break;
        case RD_IL_LSL: val = lhs->value << (rhs->value & 0x3F); break;
        case RD_IL_LSR: val = lhs->value >> (rhs->value & 0x3F); break;

        case RD_IL_ASL:
            val = (u64)((i64)lhs->value << (rhs->value & 0x3F));
            break;

        case RD_IL_ASR:
            val = (u64)((i64)lhs->value >> (rhs->value & 0x3F));
            break;

        case RD_IL_ROL: {
            u64 n = rhs->value & 0x3F;
            val = (lhs->value << n) | (lhs->value >> (64 - n));
            break;
        }

        case RD_IL_ROR: {
            u64 n = rhs->value & 0x3F;
            val = (lhs->value >> n) | (lhs->value << (64 - n));
            break;
        }

        default: unreachable(); return (RDILValue){0};
    }

    return (RDILValue){.value = val, .known = true};
}

static RDILValue _rd_il_eval_op(RDIL* self, const RDInstruction* il,
                                usize idx) {
    assert(idx < RD_MAX_OPERANDS);
    const RDOperand* op = &il->operands[idx];

    switch(op->kind) {
        case RD_OP_CNST: return (RDILValue){.value = op->cnst, .known = true};
        case RD_OP_IMM: return (RDILValue){.value = op->imm, .known = true};
        case RD_OP_ADDR: return (RDILValue){.value = op->addr, .known = true};

        case RD_OP_REG: {
            RDRegValue val;
            if(rd_il_get_regval_id(self, op->reg, &val))
                return (RDILValue){.value = val, .known = true};

            return (RDILValue){0};
        }

        case RD_OP_SYM: {
            RDRegValue val;
            if(rd_il_get_regval(self, op->sym, &val))
                return (RDILValue){.value = val, .known = true};

            return (RDILValue){0};
        }

        case RD_OP_DISPL: {
            RDRegValue base_val = 0;
            bool known = rd_il_get_regval_id(self, op->displ.base, &base_val);
            if(!known) return (RDILValue){0};

            u64 addr = base_val;

            if(op->displ.index != RD_REGID_UNKNOWN) {
                RDRegValue index_val = 0;
                known &= rd_il_get_regval_id(self, op->displ.index, &index_val);
                addr += index_val * (op->displ.scale ? op->displ.scale : 1);
            }

            addr += (RDAddress)op->displ.offset;
            return (RDILValue){.value = addr, .known = known};
        }

        case RD_OP_MEM: {
            // attempt memory read (FIXME: size unknown, use pointer size)
            RDAddress val = 0;
            if(!rd_read_ptr(self->context, op->mem, &val))
                return (RDILValue){0};

            return (RDILValue){.value = val, .known = true};
        }

        default: unreachable(); break;
    }

    return (RDILValue){0};
}

static void _rd_il_eval_set(RDIL* self, const RDOperand* dst,
                            const RDILValue* src) {
    if(dst->kind == RD_OP_REG) {
        if(src->known)
            rd_il_set_regval_id(self, dst->reg, src->value);
        else
            rd_il_del_regval_id(self, dst->reg);
    }
    else if(dst->kind == RD_OP_SYM) {
        if(src->known)
            rd_il_set_regval(self, dst->sym, src->value);
        else
            rd_il_del_regval(self, dst->sym);
    }
    // RD_OP_MEM: memory write, evaluator doesn't track memory state, skip
}

static void _rd_il_eval(RDIL* self, const RDInstruction* il) {
    switch(il->id) {
        case RD_IL_UNKNOWN:
        case RD_IL_NOP: break;

        case RD_IL_TRAP:
        case RD_IL_RET: self->done = true; break;

        case RD_IL_MOV: {
            const RDOperand* dst_op = &il->operands[0];
            RDILValue src = _rd_il_eval_op(self, il, 1);
            _rd_il_eval_set(self, dst_op, &src);
            break;
        }

        case RD_IL_PUSH: // virtual stack not implemented
            break;

        case RD_IL_POP: {
            // virtual stack not implemented.
            // Destination register becomes unknown
            if(il->operands[0].kind == RD_OP_REG)
                rd_il_del_regval_id(self, il->operands[0].reg);
            else if(il->operands[0].kind == RD_OP_SYM)
                rd_il_del_regval(self, il->operands[0].sym);
            break;
        }

        case RD_IL_JUMP:
        case RD_IL_CALL: {
            const RDOperand* op = &il->operands[0];
            if(self->lifted.real_instr.indirect && op->kind == RD_OP_MEM) {
                // indirect branch through memory pointer, use address as target
                self->target.value = op->mem;
                self->target.known = true;
            }
            else {
                RDILValue tgt = _rd_il_eval_op(self, il, 0);
                self->target.value = (RDAddress)tgt.value;
                self->target.known = tgt.known;
            }

            break;
        }

        case RD_IL_JUMP_EQ:
        case RD_IL_JUMP_NE:
        case RD_IL_JUMP_LT:
        case RD_IL_JUMP_LE:
        case RD_IL_JUMP_GT:
        case RD_IL_JUMP_GE:
        case RD_IL_CALL_EQ:
        case RD_IL_CALL_NE:
        case RD_IL_CALL_LT:
        case RD_IL_CALL_LE:
        case RD_IL_CALL_GT:
        case RD_IL_CALL_GE: {
            RDILValue lhs = _rd_il_eval_op(self, il, 0);
            RDILValue rhs = _rd_il_eval_op(self, il, 1);

            if(lhs.known && rhs.known && _rd_il_eval_cond(il, &lhs, &rhs)) {
                RDILValue tgt = _rd_il_eval_op(self, il, 2);
                self->target.value = (RDAddress)tgt.value;
                self->target.known = tgt.known;
            }

            break;
        }

        // ALU ops
        case RD_IL_NOT: {
            const RDOperand* dst_op = &il->operands[0];
            RDILValue src = _rd_il_eval_op(self, il, 1);
            if(src.known) src.value = ~src.value;
            _rd_il_eval_set(self, dst_op, &src);
            break;
        }

        case RD_IL_ADD:
        case RD_IL_SUB:
        case RD_IL_MUL:
        case RD_IL_DIV:
        case RD_IL_MOD:
        case RD_IL_AND:
        case RD_IL_OR:
        case RD_IL_XOR:
        case RD_IL_LSL:
        case RD_IL_LSR:
        case RD_IL_ASL:
        case RD_IL_ASR:
        case RD_IL_ROL:
        case RD_IL_ROR: {
            const RDOperand* dst_op = &il->operands[0];
            RDILValue src1 = _rd_il_eval_op(self, il, 1);
            RDILValue src2 = _rd_il_eval_op(self, il, 2);
            RDILValue res = _rd_il_eval_alu(il, &src1, &src2);
            _rd_il_eval_set(self, dst_op, &res);
            break;
        }

        default: unreachable();
    }
}

RDIL* rd_il_create(RDContext* ctx, const RDFunction* f) {
    RDIL* self = rd_alloc(sizeof(*self));
    *self = (RDIL){.context = ctx};
    rd_i_registermap_init(&self->registers);
    rd_il_assign(self, f);
    return self;
}

void rd_il_destroy(RDIL* self) {
    if(!self) return;

    hmap_destroy(&self->registers);
    vect_destroy(&self->lifted);
    rd_free(self);
}

void rd_il_flush(RDIL* self) {
    hmap_clear(&self->registers);
    vect_clear(&self->lifted);
    self->current_address = self->function ? self->function->address : 0;
    self->done = false;
}

void rd_il_assign(RDIL* self, const RDFunction* f) {
    self->function = f;
    rd_il_flush(self);

    if(!f) return;

    // seed segment registers from DB into local register file
    const RDSegmentRegNameVect* sreg_names =
        rd_i_db_get_all_sreg_names(self->context);

    const char** it;
    vect_each(it, sreg_names) {
        RDRegValue val;

        if(rd_get_sregval(self->context, f->address, *it, &val))
            rd_il_set_regval(self, *it, val);
    }
}

bool rd_il_run(RDIL* self) {
    if(!self->function) return false;
    if(!self->context->processorplugin->lift) return false;

    while(rd_il_step(self))
        ;

    return true;
}

bool rd_il_step(RDIL* self) {
    if(self->done || !self->function || !self->function->graph) return false;

    self->target.known = false;

    const RDInstructionVect* v =
        rd_il_lift(self->context, self->current_address, &self->lifted);

    RDAddress next_address = self->current_address + v->real_instr.length;

    // this instruction has delay slots:
    // save the address, process delay slots and restore the address
    if(v->real_instr.delay_slots > 0 &&
       v->real_instr.delay_slots != RD_IS_DSLOT) {

        // keep lifted instruction with delay slot and restore it later
        RDInstructionVect keep = *v;
        bool done_keep = self->done;
        self->lifted = (RDInstructionVect){0};
        self->current_address = self->current_address + keep.real_instr.length;

        for(u8 i = 0; i < keep.real_instr.delay_slots; i++)
            rd_il_step(self);

        vect_destroy(&self->lifted);
        self->lifted = keep;
        self->done = done_keep;
        next_address = self->current_address;
    }

    const RDInstruction* il;
    vect_each(il, &self->lifted) {
        _rd_il_eval(self, il);
        if(self->done) return true;
    }

    self->current_address = next_address;

    // check if we've left the function
    if(!rd_function_contains_address(self->function, self->current_address) ||
       !rd_instr_can_flow(&self->lifted.real_instr))
        self->done = true;

    return true;
}

RDAddress rd_il_current_address(const RDIL* self) {
    return self->current_address;
}

RDILInstructionSlice rd_il_current_il(const RDIL* self) {
    return vect_to_slice(RDILInstructionSlice, &self->lifted);
}

const RDInstruction* rd_il_current_instr(const RDIL* self) {
    return &self->lifted.real_instr;
}

bool rd_il_get_target(const RDIL* self, RDAddress* target) {
    if(self->target.known && target) *target = self->target.value;
    return self->target.known;
}

bool rd_il_get_regval(const RDIL* self, const char* regname,
                      RDRegValue* value) {
    if(!regname) return false;

    RDContext* ctx = self->context;
    RDRegMask m = {.mask = RD_REGMASK_FULL, .shift = 0};

    if(ctx->processorplugin->get_reg_mask)
        ctx->processorplugin->get_reg_mask(regname, &m, ctx->processor);

    const char* canonical = regname;
    if(m.mask != RD_REGMASK_FULL && ctx->processorplugin->get_reg_name)
        canonical = ctx->processorplugin->get_reg_name(m.reg, ctx->processor);

    RDRegister key = {.name = rd_i_strpool_intern(&ctx->strings, canonical)};
    const RDRegister* r = hmap_get(&self->registers, &key);
    if(!r) return false;

    if(value) *value = (r->value & m.mask) >> m.shift;
    return true;
}

bool rd_il_set_regval(RDIL* self, const char* regname, RDRegValue value) {
    if(!regname) return false;

    RDContext* ctx = self->context;
    RDRegMask m = {.mask = RD_REGMASK_FULL, .shift = 0};

    if(ctx->processorplugin->get_reg_mask)
        ctx->processorplugin->get_reg_mask(regname, &m, ctx->processor);

    const char* canonical = regname;
    if(m.mask != RD_REGMASK_FULL && ctx->processorplugin->get_reg_name)
        canonical = ctx->processorplugin->get_reg_name(m.reg, ctx->processor);

    // merge if bit field
    if(m.mask != RD_REGMASK_FULL) {
        RDRegValue base = 0;
        rd_il_get_regval(self, canonical, &base);
        value = (base & ~m.mask) | ((value << m.shift) & m.mask);
    }

    RDRegister r = {
        .name = rd_i_strpool_intern(&ctx->strings, canonical),
        .value = value,
    };

    hmap_set(&self->registers, &r);
    return true;
}

bool rd_il_del_regval(RDIL* self, const char* regname) {
    if(!regname) return false;

    RDContext* ctx = self->context;
    RDRegMask m = {.mask = RD_REGMASK_FULL, .shift = 0};

    if(ctx->processorplugin->get_reg_mask)
        ctx->processorplugin->get_reg_mask(regname, &m, ctx->processor);

    const char* canonical = regname;
    if(m.mask != RD_REGMASK_FULL && ctx->processorplugin->get_reg_name)
        canonical = ctx->processorplugin->get_reg_name(m.reg, ctx->processor);

    RDRegister key = {.name = rd_i_strpool_intern(&ctx->strings, canonical)};
    hmap_del(&self->registers, &key);
    return true;
}

bool rd_il_get_regval_id(const RDIL* self, RDReg id, RDRegValue* value) {
    RDContext* ctx = self->context;
    if(!ctx->processorplugin->get_reg_name) return false;

    const char* regname =
        ctx->processorplugin->get_reg_name(id, ctx->processor);
    return regname && rd_il_get_regval(self, regname, value);
}

bool rd_il_set_regval_id(RDIL* self, RDReg id, RDRegValue value) {
    RDContext* ctx = self->context;
    if(!ctx->processorplugin->get_reg_name) return false;

    const char* regname =
        ctx->processorplugin->get_reg_name(id, ctx->processor);
    return regname && rd_il_set_regval(self, regname, value);
}

bool rd_il_del_regval_id(RDIL* self, RDReg id) {
    RDContext* ctx = self->context;
    if(!ctx->processorplugin->get_reg_name) return false;

    const char* regname =
        ctx->processorplugin->get_reg_name(id, ctx->processor);
    return regname && rd_il_del_regval(self, regname);
}

static bool _rd_il_invalid_operand(const RDInstruction* instr,
                                   RDAddress address, int instr_idx,
                                   int op_idx) {
    const RDOperand* op = &instr->operands[op_idx];
    const char* op_kind = NULL;

    switch(op->kind) {
        case RD_OP_NULL: op_kind = "NULL"; break;
        case RD_OP_CNST: op_kind = "CNST"; break;
        case RD_OP_REG: op_kind = "REG"; break;
        case RD_OP_IMM: op_kind = "IMM"; break;
        case RD_OP_ADDR: op_kind = "ADDR"; break;
        case RD_OP_MEM: op_kind = "MEM"; break;
        case RD_OP_DISPL: op_kind = "DISPL"; break;
        case RD_OP_SYM: op_kind = "SYM"; break;
        default: op_kind = "USER"; break;
    }

    RD_LOG_FAIL(
        "RDIL instruction #%d '%s' at %llx, invalid operand %d (kind = %s)",
        instr_idx, instr->mnemonic, address, op_idx, op_kind);

    return false;
}

static bool _rd_il_validate(RDAddress address, const RDInstructionVect* v) {
    for(int i = 0; i < (int)vect_length(v); i++) {
        const RDInstruction* instr = vect_at(v, i);

        switch(instr->id) {
            case RD_IL_CALL:
            case RD_IL_JUMP:
            case RD_IL_PUSH: {
                if(instr->operands[0].kind == RD_OP_NULL)
                    return _rd_il_invalid_operand(instr, address, i, 0);

                break;
            }

            case RD_IL_POP: {
                RDOperandKind k = instr->operands[0].kind;

                if(k != RD_OP_REG && k != RD_OP_SYM)
                    return _rd_il_invalid_operand(instr, address, i, 0);

                break;
            }

            case RD_IL_MOV: {
                RDOperandKind k = instr->operands[0].kind;

                if(k != RD_OP_REG && k != RD_OP_MEM && k != RD_OP_SYM)
                    return _rd_il_invalid_operand(instr, address, i, 0);

                if(instr->operands[1].kind == RD_OP_NULL)
                    return _rd_il_invalid_operand(instr, address, i, 1);

                break;
            }

            case RD_IL_CALL_EQ:
            case RD_IL_CALL_NE:
            case RD_IL_CALL_LT:
            case RD_IL_CALL_LE:
            case RD_IL_CALL_GT:
            case RD_IL_CALL_GE:
            case RD_IL_JUMP_EQ:
            case RD_IL_JUMP_NE:
            case RD_IL_JUMP_LT:
            case RD_IL_JUMP_LE:
            case RD_IL_JUMP_GT:
            case RD_IL_JUMP_GE: {
                if(instr->operands[0].kind == RD_OP_NULL)
                    return _rd_il_invalid_operand(instr, address, i, 0);

                if(instr->operands[1].kind == RD_OP_NULL)
                    return _rd_il_invalid_operand(instr, address, i, 1);

                if(instr->operands[2].kind == RD_OP_NULL)
                    return _rd_il_invalid_operand(instr, address, i, 2);

                break;
            }

            case RD_IL_NOT: {
                RDOperandKind k = instr->operands[0].kind;

                if(k != RD_OP_REG && k != RD_OP_SYM)
                    return _rd_il_invalid_operand(instr, address, i, 0);

                if(instr->operands[1].kind == RD_OP_NULL)
                    return _rd_il_invalid_operand(instr, address, i, 1);

                break;
            }

            case RD_IL_ADD:
            case RD_IL_SUB:
            case RD_IL_MUL:
            case RD_IL_DIV:
            case RD_IL_MOD:
            case RD_IL_AND:
            case RD_IL_OR:
            case RD_IL_XOR:
            case RD_IL_LSL:
            case RD_IL_LSR:
            case RD_IL_ASL:
            case RD_IL_ASR:
            case RD_IL_ROL:
            case RD_IL_ROR: {
                RDOperandKind k = instr->operands[0].kind;

                if(k != RD_OP_REG && k != RD_OP_SYM)
                    return _rd_il_invalid_operand(instr, address, i, 0);

                if(instr->operands[1].kind == RD_OP_NULL)
                    return _rd_il_invalid_operand(instr, address, i, 1);

                if(instr->operands[2].kind == RD_OP_NULL)
                    return _rd_il_invalid_operand(instr, address, i, 2);

                break;
            }

            default: break;
        }
    }

    return true;
}

const RDInstructionVect* rd_il_lift(RDContext* ctx, RDAddress address,
                                    RDInstructionVect* v) {
    vect_clear(v);

    if(ctx->processorplugin->lift) {
        if(rd_decode(ctx, address, &v->real_instr))
            ctx->processorplugin->lift(ctx, v, &v->real_instr, ctx->processor);
    }

    if(!_rd_il_validate(address, v)) vect_clear(v);
    if(vect_is_empty(v)) rd_il_push_instr(v, RD_IL_UNKNOWN);

    return v;
}

RDILInstructionSlice rd_lift(RDContext* ctx, RDAddress address) {
    const RDInstructionVect* il = rd_il_lift(ctx, address, &ctx->lift_buf);

    return (RDILInstructionSlice){
        .data = il->data,
        .length = il->length,
        .instruction_length = il->real_instr.length,
    };
}

RDInstruction* rd_il_push_instr(RDInstructionVect* self, RDILStatement s) {
    if(s >= RD_IL_COUNT) {
        RD_LOG_FAIL("%d is an invalid RDIL statement", (int)s);
        return NULL;
    }

    vect_push(self, (RDInstruction){.id = (u32)s, .flow = RD_IF_RDIL});
    RDInstruction* instr = vect_last(self);

    const RDILStatementInfo* info = &RDIL_OP_TABLE[instr->id];
    instr->mnemonic = info->mnemonic;
    return instr;
}

bool rd_il_instr_match(const RDContext* ctx, const RDILInstructionSlice* il,
                       const RDInstruction* shapes, usize count) {
    if(!count || !il->length || il->length != count) return false;

    for(usize i = 0; i < count; i++) {
        if(!rd_instr_match(ctx, &il->data[i], &shapes[i])) return false;
    }

    return true;
}
