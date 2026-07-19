#include "symbol.h"
#include "core/context.h"
#include "core/segment.h"
#include "db/db.h"
#include "support/containers.h"

static const char* _rd_symbol_extract_string(const RDSymbol* self,
                                             RDContext* ctx) {
    const RDSegmentFull* seg = rd_i_db_find_segment(ctx, self->address);
    assert(seg && "cannot convert symbol to string, type outside of segments");

    usize idx = rd_i_address2index(seg, self->address);

    // reserve at least these bytes, +2 for quoting, +1 null terminator
    vect_reserve(&ctx->sym_buf, self->type.count + 3);
    vect_clear(&ctx->sym_buf);
    vect_push(&ctx->sym_buf, '\"');

    if(!strcmp(self->type.def->name, "char16")) {
        for(usize i = 0; i < self->type.count; i++) {
            u8 lo = 0, hi = 0;

            bool ok =
                rd_flagsbuffer_get_value(seg->flags, idx + (i * 2), &lo) &&
                rd_flagsbuffer_get_value(seg->flags, idx + (i * 2) + 1, &hi);
            assert(ok && "cannot convert symbol to string, missing character");

            u16 v = (u16)(lo | ((u16)hi << 8));
            if(!v) break;

            const char* s = rd_i_escape_char16(v, true);
            while(*s)
                vect_push(&ctx->sym_buf, *s++);
        }
    }
    else {
        for(usize i = 0; i < self->type.count; i++) {
            u8 v = 0;
            bool ok = rd_flagsbuffer_get_value(seg->flags, idx + i, &v);
            assert(ok && "cannot convert symbol to string, missing character");

            if(!v) break;

            const char* s = rd_i_escape_char((char)v, true);
            while(*s)
                vect_push(&ctx->sym_buf, *s++);
        }
    }

    vect_push(&ctx->sym_buf, '\"');
    vect_push(&ctx->sym_buf, 0);
    return ctx->sym_buf.data;
}

int rd_i_symbol_sort_pred(const void* a, const void* b) {
    const RDSymbol* sym_a = (const RDSymbol*)a;
    const RDSymbol* sym_b = (const RDSymbol*)b;
    if(sym_a->address < sym_b->address) return -1;
    if(sym_a->address > sym_b->address) return 1;
    if(sym_a->kind < sym_b->kind) return -1;
    if(sym_a->kind > sym_b->kind) return 1;
    return 0;
}

const char* rd_symbol_to_string(const RDSymbol* self, RDContext* ctx) {
    switch(self->kind) {
        case RD_SYMBOL_SEGMENT: {
            const RDSegmentFull* seg = rd_i_db_find_segment(ctx, self->address);
            return seg ? seg->base.name : NULL;
        }

        case RD_SYMBOL_TYPE: {
            if(rd_type_is_string(&self->type))
                return _rd_symbol_extract_string(self, ctx);

            break;
        }

        default: break;
    }

    return rd_get_name(ctx, self->address);
}
