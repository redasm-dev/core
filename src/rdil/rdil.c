#include "rdil.h"
#include "support/containers.h"
#include "surface/renderer.h"

static u8 _rdil_read_u8(const RDILInstruction* self, usize* cur) {
    assert(*cur < self->length && "RDIL buffer overrun");
    return self->data[(*cur)++];
}

static u64 _rdil_read_uleb(const RDILInstruction* self, usize* cur) {
    u64 result = 0;
    int shift = 0;
    u8 b;

    do {
        b = _rdil_read_u8(self, cur);
        result |= (u64)(b & 0x7F) << shift;
        shift += 7;
    } while(b & 0x80);

    return result;
}

static i64 _rdil_read_sleb(const RDILInstruction* self, usize* cur) {
    i64 result = 0;
    int shift = 0;
    u8 b;

    do {
        b = _rdil_read_u8(self, cur);
        result |= (i64)(b & 0x7F) << shift;
        shift += 7;
    } while(b & 0x80);

    // sign extend if sign bit of last byte was set
    if((b & 0x40) && shift < 64) result |= -(1LL << shift);
    return result;
}

static i64 _rdil_peek_sleb(const RDILInstruction* self, usize cur) {
    i64 result = 0;
    int shift = 0;
    u8 b;

    do {
        b = _rdil_read_u8(self, &cur);
        result |= (i64)(b & 0x7F) << shift;
        shift += 7;
    } while(b & 0x80);

    if((b & 0x40) && shift < 64) result |= -(1LL << shift);

    return result;
}

static void _rdil_write_u8(RDILInstruction* self, u8 b) { vect_push(self, b); }

static void _rdil_write_uleb(RDILInstruction* self, u64 v) {
    do {
        u8 b = v & 0x7F;
        v >>= 7;
        if(v) b |= 0x80;
        _rdil_write_u8(self, b);
    } while(v);
}

static void _rdil_write_sleb(RDILInstruction* self, i64 v) {
    bool more = true;
    while(more) {
        u8 b = v & 0x7F;
        v >>= 7;
        // sign bit of b matches sign of remaining value -> done
        bool sign = b & 0x40;
        more = !((v == 0 && !sign) || (v == -1 && sign));
        if(more) b |= 0x80;

        _rdil_write_u8(self, b);
    }
}

void rd_i_il_init(RDILInstruction* self) { vect_clear(self); }
void rd_i_il_deinit(RDILInstruction* self) { vect_destroy(self); }

void rd_il_nop(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_NOP); }
void rd_il_unkn(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_UNKN); }
void rd_il_copy(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_COPY); }
void rd_il_goto(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_GOTO); }
void rd_il_call(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_CALL); }
void rd_il_ret(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_RET); }
void rd_il_if(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_IF); }
void rd_il_push(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_PUSH); }
void rd_il_pop(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_POP); }
void rd_il_trap(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_TRAP); }

void rd_il_reg(RDILInstruction* self, int reg) {
    _rdil_write_u8(self, RD_IL_REG);
    _rdil_write_uleb(self, (u64)reg);
}

void rd_il_var(RDILInstruction* self, RDAddress address) {
    _rdil_write_u8(self, RD_IL_VAR);
    _rdil_write_uleb(self, address);
}

void rd_il_sym(RDILInstruction* self, const char* name) {
    _rdil_write_u8(self, RD_IL_SYM);
    while(*name)
        _rdil_write_u8(self, (u8)*name++);
    _rdil_write_u8(self, 0);
}

void rd_il_uint(RDILInstruction* self, u64 value) {
    _rdil_write_u8(self, RD_IL_UINT);
    _rdil_write_uleb(self, value);
}

void rd_il_sint(RDILInstruction* self, i64 value) {
    _rdil_write_u8(self, RD_IL_SINT);
    _rdil_write_sleb(self, value);
}

void rd_il_mem(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_MEM); }
void rd_il_not(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_NOT); }
void rd_il_add(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_ADD); }
void rd_il_sub(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_SUB); }
void rd_il_mul(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_MUL); }
void rd_il_div(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_DIV); }
void rd_il_mod(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_MOD); }
void rd_il_and(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_AND); }
void rd_il_or(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_OR); }
void rd_il_xor(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_XOR); }
void rd_il_lsl(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_LSL); }
void rd_il_lsr(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_LSR); }
void rd_il_asl(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_ASL); }
void rd_il_asr(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_ASR); }
void rd_il_rol(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_ROL); }
void rd_il_ror(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_ROR); }
void rd_il_eq(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_EQ); }
void rd_il_ne(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_NE); }
void rd_il_lt(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_LT); }
void rd_il_le(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_LE); }
void rd_il_gt(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_GT); }
void rd_il_ge(RDILInstruction* self) { _rdil_write_u8(self, RD_IL_GE); }

static const char* _rdil_op_str(u8 op) {
    switch(op) {
        case RD_IL_ADD: return "+";
        case RD_IL_SUB: return "-";
        case RD_IL_MUL: return "*";
        case RD_IL_DIV: return "/";
        case RD_IL_MOD: return "%";
        case RD_IL_AND: return "&";
        case RD_IL_OR: return "|";
        case RD_IL_XOR: return "^";
        case RD_IL_LSL: return "<<";
        case RD_IL_LSR: return ">>";
        case RD_IL_ASL: return "<<<";
        case RD_IL_ASR: return ">>>";
        case RD_IL_ROL: return "rol ";
        case RD_IL_ROR: return "ror ";
        case RD_IL_EQ: return "==";
        case RD_IL_NE: return "!=";
        case RD_IL_LT: return "<";
        case RD_IL_LE: return "<=";
        case RD_IL_GT: return ">";
        case RD_IL_GE: return ">=";
        default: return "?";
    }
}

static void _rdil_render_expr(RDRenderer* r, const RDILInstruction* il,
                              usize* cur) {
    u8 op = _rdil_read_u8(il, cur);

    switch(op) {
        case RD_IL_REG: {
            int reg = (int)_rdil_read_uleb(il, cur);
            rd_renderer_reg(r, reg);
            break;
        }

        case RD_IL_VAR: {
            RDAddress addr = _rdil_read_uleb(il, cur);
            const char* name = rd_get_name(r->context, addr);
            if(name) {
                rd_renderer_text(r, name, RD_THEME_LOCATION,
                                 RD_THEME_BACKGROUND);
            }
            else
                rd_renderer_loc(r, addr, 0, RD_NUM_DEFAULT);
            break;
        }

        case RD_IL_SYM: {
            const char* name = (const char*)(il->data + *cur);
            rd_renderer_text(r, name, RD_THEME_LOCATION, RD_THEME_BACKGROUND);
            *cur += strlen(name) + 1;
            break;
        }

        case RD_IL_UINT: {
            u64 v = _rdil_read_uleb(il, cur);
            rd_renderer_num(r, v, 16, 0, RD_NUM_DEFAULT);
            break;
        }

        case RD_IL_SINT: {
            i64 v = _rdil_read_sleb(il, cur);
            rd_renderer_num(r, v < 0 ? -v : v, 16, 0, RD_NUM_DEFAULT);
            break;
        }

        case RD_IL_MEM:
            rd_renderer_norm(r, "[");
            _rdil_render_expr(r, il, cur); // u
            rd_renderer_norm(r, "]");
            break;

        case RD_IL_NOT:
            rd_renderer_norm(r, "~");
            _rdil_render_expr(r, il, cur); // u
            break;

        case RD_IL_ADD: {
            _rdil_render_expr(r, il, cur); // l
            if(il->data[*cur] == RD_IL_SINT &&
               _rdil_peek_sleb(il, *cur + 1) < 0)
                rd_renderer_norm(r, "-");
            else
                rd_renderer_norm(r, "+");
            _rdil_render_expr(r, il, cur); // r
            break;
        }

        case RD_IL_SUB: {
            _rdil_render_expr(r, il, cur); // l
            if(il->data[*cur] == RD_IL_SINT &&
               _rdil_peek_sleb(il, *cur + 1) < 0)
                rd_renderer_norm(r, "+");
            else
                rd_renderer_norm(r, "-");
            _rdil_render_expr(r, il, cur); // r
            break;
        }

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
        case RD_IL_ROR:
        case RD_IL_EQ:
        case RD_IL_NE:
        case RD_IL_LT:
        case RD_IL_LE:
        case RD_IL_GT:
        case RD_IL_GE: {
            _rdil_render_expr(r, il, cur); // l
            rd_renderer_norm(r, _rdil_op_str(op));
            _rdil_render_expr(r, il, cur); // r
            break;
        }

        default: rd_renderer_norm(r, "?"); break;
    }
}

static void _rdil_render_statement(RDRenderer* r, const RDILInstruction* il,
                                   usize* cur) {
    u8 op = _rdil_read_u8(il, cur);

    switch(op) {

        case RD_IL_NOP: rd_renderer_nop(r, "nop"); break;
        case RD_IL_UNKN: rd_renderer_nop(r, "unknown"); break;

        case RD_IL_COPY: {
            _rdil_render_expr(r, il, cur); // dst
            rd_renderer_norm(r, "=");
            _rdil_render_expr(r, il, cur); // src
            break;
        }

        case RD_IL_GOTO: {
            rd_renderer_text(r, "goto", RD_THEME_JUMP, RD_THEME_BACKGROUND);
            rd_renderer_ws(r, 1);
            _rdil_render_expr(r, il, cur); // target
            break;
        }

        case RD_IL_CALL: {
            _rdil_render_expr(r, il, cur); // target (VAR renders as name)
            rd_renderer_norm(r, "()");
            break;
        }

        case RD_IL_RET: {
            rd_renderer_text(r, "ret", RD_THEME_STOP, RD_THEME_BACKGROUND);
            rd_renderer_norm(r, " ");
            _rdil_render_expr(r, il, cur); // target
            break;
        }

        case RD_IL_IF: {
            rd_renderer_text(r, "if", RD_THEME_JUMP_COND, RD_THEME_BACKGROUND);
            rd_renderer_norm(r, "(");
            _rdil_render_expr(r, il, cur); // cond
            rd_renderer_norm(r, ") ");
            rd_renderer_text(r, "goto", RD_THEME_JUMP, RD_THEME_BACKGROUND);
            rd_renderer_ws(r, 1);
            _rdil_render_expr(r, il, cur); // t
            rd_renderer_norm(r, " else ");
            rd_renderer_text(r, "goto", RD_THEME_JUMP, RD_THEME_BACKGROUND);
            rd_renderer_ws(r, 1);
            _rdil_render_expr(r, il, cur); // f
            break;
        }

        case RD_IL_PUSH: {
            rd_renderer_norm(r, "push");
            rd_renderer_norm(r, "(");
            _rdil_render_expr(r, il, cur); // src
            rd_renderer_norm(r, ")");
            break;
        }

        case RD_IL_POP: {
            _rdil_render_expr(r, il, cur); // dst
            rd_renderer_norm(r, "=pop()");
            break;
        }

        case RD_IL_TRAP: {
            rd_renderer_norm(r, "trap");
            rd_renderer_ws(r, 1);
            _rdil_render_expr(r, il, cur); // u
            break;
        }

        default: rd_renderer_norm(r, "?"); break;
    }
}
void rd_i_il_render(RDRenderer* r, const RDILInstruction* il) {
    if(!il->length) {
        rd_renderer_nop(r, "unknown");
        return;
    }

    usize cur = 0;
    bool first = true;

    while(cur < il->length) {
        if(!first) rd_renderer_norm(r, "; ");
        _rdil_render_statement(r, il, &cur);
        first = false;
    }
}
