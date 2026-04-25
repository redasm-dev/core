#include "flagsbuffer.h"
#include "io/flags.h"
#include <stdlib.h>

static inline RDFlagsBuffer* _rd_as_flagsbuffer(RDBuffer* self) {
    return (RDFlagsBuffer*)self;
}

static inline const RDFlagsBuffer* _rd_as_flagsbuffer_c(const RDBuffer* self) {
    return (const RDFlagsBuffer*)self;
}

static bool _rd_flagsbuffer_get_byte(const RDBuffer* self, usize idx, u8* b) {
    return idx < self->length &&
           rd_i_flags_get_value(_rd_as_flagsbuffer_c(self)->data[idx], b);
}

static bool _rd_flagsbuffer_set_byte(RDBuffer* self, usize idx, u8 b) {
    if(idx < self->length) {
        rd_i_flags_set_value(&_rd_as_flagsbuffer(self)->data[idx], b);
        return true;
    }

    return false;
}

static void _rd_flagsbuffer_destroy(RDBuffer* self) {
    free(_rd_as_flagsbuffer(self)->data);
    free(self);
}

static void _rd_flagsbuffer_set_tails(RDFlagsBuffer* self, usize idx, usize n) {
    for(usize i = 0; i < n; i++)
        rd_i_flags_set_tail(&self->data[idx + i]);
}

RDFlagsBuffer* rd_i_flagsbuffer_create(usize n) {
    RDFlagsBuffer* self = calloc(1, sizeof(*self));
    self->base.get_byte = _rd_flagsbuffer_get_byte;
    self->base.set_byte = _rd_flagsbuffer_set_byte;
    self->base.destroy = _rd_flagsbuffer_destroy;
    self->base.length = n;
    self->data = calloc(n, sizeof(*self->data));
    return self;
}

usize rd_flagsbuffer_get_length(const RDFlagsBuffer* self) {
    return rd_i_buffer_get_length((const RDBuffer*)self);
}

bool rd_flagsbuffer_has_unknown(const RDFlagsBuffer* self, usize idx) {
    return idx < self->base.length && rd_i_flags_has_unknown(self->data[idx]);
}

bool rd_i_flagsbuffer_has_info(const RDFlagsBuffer* self, usize idx) {
    return idx < self->base.length && rd_i_flags_has_info(self->data[idx]);
}

bool rd_flagsbuffer_has_code(const RDFlagsBuffer* self, usize idx) {
    return idx < self->base.length && rd_i_flags_has_code(self->data[idx]);
}

bool rd_flagsbuffer_has_data(const RDFlagsBuffer* self, usize idx) {
    return idx < self->base.length && rd_i_flags_has_data(self->data[idx]);
}

bool rd_flagsbuffer_has_tail(const RDFlagsBuffer* self, usize idx) {
    return idx < self->base.length && rd_i_flags_has_tail(self->data[idx]);
}

bool rd_flagsbuffer_has_name(const RDFlagsBuffer* self, usize idx) {
    return idx < self->base.length && rd_i_flags_has_name(self->data[idx]);
}

bool rd_i_flagsbuffer_has_comment(const RDFlagsBuffer* self, usize idx) {
    return idx < self->base.length && rd_i_flags_has_comment(self->data[idx]);
}

bool rd_flagsbuffer_has_flow(const RDFlagsBuffer* self, usize idx) {
    return idx < self->base.length && rd_i_flags_has_flow(self->data[idx]);
}

bool rd_flagsbuffer_has_jump(const RDFlagsBuffer* self, usize idx) {
    return idx < self->base.length && rd_i_flags_has_jump(self->data[idx]);
}

bool rd_i_flagsbuffer_has_jmpdst(const RDFlagsBuffer* self, usize idx) {
    return idx < self->base.length && rd_i_flags_has_jmpdst(self->data[idx]);
}

bool rd_flagsbuffer_has_call(const RDFlagsBuffer* self, usize idx) {
    return idx < self->base.length && rd_i_flags_has_call(self->data[idx]);
}

bool rd_flagsbuffer_has_func(const RDFlagsBuffer* self, usize idx) {
    return idx < self->base.length && rd_i_flags_has_func(self->data[idx]);
}

bool rd_flagsbuffer_has_noret(const RDFlagsBuffer* self, usize idx) {
    return idx < self->base.length && rd_i_flags_has_noret(self->data[idx]);
}

bool rd_flagsbuffer_has_cond(const RDFlagsBuffer* self, usize idx) {
    return idx < self->base.length && rd_i_flags_has_cond(self->data[idx]);
}

bool rd_i_flagsbuffer_has_type(const RDFlagsBuffer* self, usize idx) {
    return idx < self->base.length && rd_i_flags_has_type(self->data[idx]);
}

bool rd_i_flagsbuffer_has_xref_out(const RDFlagsBuffer* self, usize idx) {
    return idx < self->base.length && rd_i_flags_has_xref_out(self->data[idx]);
}

bool rd_i_flagsbuffer_has_xref_in(const RDFlagsBuffer* self, usize idx) {
    return idx < self->base.length && rd_i_flags_has_xref_in(self->data[idx]);
}

bool rd_flagsbuffer_has_imported(const RDFlagsBuffer* self, usize idx) {
    return idx < self->base.length && rd_i_flags_has_imported(self->data[idx]);
}

bool rd_flagsbuffer_has_exported(const RDFlagsBuffer* self, usize idx) {
    return idx < self->base.length && rd_i_flags_has_exported(self->data[idx]);
}

bool rd_flagsbuffer_get_value(const RDFlagsBuffer* self, usize idx, u8* v) {
    return idx < self->base.length && rd_i_flags_get_value(self->data[idx], v);
}

bool rd_i_flagsbuffer_set_value(RDFlagsBuffer* self, usize idx, u8 v) {
    if(idx < self->base.length) {
        rd_i_flags_set_value(&self->data[idx], v);
        return true;
    }

    return false;
}

bool rd_i_flagsbuffer_has_unknown_n(const RDFlagsBuffer* self, usize startidx,
                                    usize n) {
    usize endidx = startidx + n;
    if(endidx > self->base.length) endidx = self->base.length;

    for(usize idx = startidx; idx < endidx; idx++) {
        if(!rd_i_flags_has_unknown(self->data[idx])) return false;
    }

    return true;
}

bool rd_i_flagsbuffer_has_code_n(const RDFlagsBuffer* self, usize startidx,
                                 usize n) {
    usize endidx = startidx + n;
    if(endidx > self->base.length) endidx = self->base.length;

    for(usize idx = startidx; idx < endidx; idx++) {
        if(rd_i_flags_has_code(self->data[idx])) return true;
    }

    return false;
}

bool rd_i_flagsbuffer_has_data_n(const RDFlagsBuffer* self, usize startidx,
                                 usize n) {
    usize endidx = startidx + n;
    if(endidx > self->base.length) endidx = self->base.length;

    for(usize idx = startidx; idx < endidx; idx++) {
        if(rd_i_flags_has_data(self->data[idx])) return true;
    }

    return false;
}

bool rd_i_flagsbuffer_set_data(RDFlagsBuffer* self, usize idx, usize n) {
    if(n && (idx + n <= self->base.length)) {
        rd_i_flags_set_data(&self->data[idx]);
        _rd_flagsbuffer_set_tails(self, idx + 1, n - 1);
        return true;
    }

    return false;
}

bool rd_i_flagsbuffer_set_code(RDFlagsBuffer* self, usize idx, usize n) {
    if(n && (idx + n <= self->base.length)) {
        rd_i_flags_set_code(&self->data[idx]);
        _rd_flagsbuffer_set_tails(self, idx + 1, n - 1);
        return true;
    }

    return false;
}

bool rd_i_flagsbuffer_set_flow(RDFlagsBuffer* self, usize idx) {
    if(idx < self->base.length) {
        rd_i_flags_set_flow(&self->data[idx]);
        return true;
    }

    return false;
}

bool rd_i_flagsbuffer_set_jump(RDFlagsBuffer* self, usize idx) {
    if(idx < self->base.length) {
        rd_i_flags_set_jump(&self->data[idx]);
        return true;
    }

    return false;
}

bool rd_i_flagsbuffer_set_jmpdst(RDFlagsBuffer* self, usize idx) {
    if(idx < self->base.length) {
        rd_i_flags_set_jmpdst(&self->data[idx]);
        return true;
    }

    return false;
}

bool rd_i_flagsbuffer_set_call(RDFlagsBuffer* self, usize idx) {
    if(idx < self->base.length) {
        rd_i_flags_set_call(&self->data[idx]);
        return true;
    }

    return false;
}

bool rd_i_flagsbuffer_set_func(RDFlagsBuffer* self, usize idx) {
    if(idx < self->base.length) {
        rd_i_flags_set_func(&self->data[idx]);
        return true;
    }

    return false;
}

bool rd_i_flagsbuffer_set_noret(RDFlagsBuffer* self, usize idx) {
    if(idx < self->base.length) {
        rd_i_flags_set_noret(&self->data[idx]);
        return true;
    }

    return false;
}

bool rd_i_flagsbuffer_set_cond(RDFlagsBuffer* self, usize idx) {
    if(idx < self->base.length) {
        rd_i_flags_set_cond(&self->data[idx]);
        return true;
    }

    return false;
}

bool rd_i_flagsbuffer_set_type(RDFlagsBuffer* self, usize idx, usize n) {
    if(idx + n <= self->base.length) {
        rd_i_flags_set_data(&self->data[idx]);
        rd_i_flags_set_type(&self->data[idx]);
        _rd_flagsbuffer_set_tails(self, idx + 1, n - 1);
        return true;
    }

    return false;
}

bool rd_i_flagsbuffer_set_name(RDFlagsBuffer* self, usize idx) {
    if(idx < self->base.length) {
        rd_i_flags_set_name(&self->data[idx]);
        return true;
    }

    return false;
}

bool rd_i_flagsbuffer_set_comment(RDFlagsBuffer* self, usize idx) {
    if(idx < self->base.length) {
        rd_i_flags_set_comment(&self->data[idx]);
        return true;
    }

    return false;
}

bool rd_i_flagsbuffer_set_xref_out(RDFlagsBuffer* self, usize idx) {
    if(idx < self->base.length) {
        rd_i_flags_set_xref_out(&self->data[idx]);
        return true;
    }

    return false;
}

bool rd_i_flagsbuffer_set_xref_in(RDFlagsBuffer* self, usize idx) {
    if(idx < self->base.length) {
        rd_i_flags_set_xref_in(&self->data[idx]);
        return true;
    }

    return false;
}

bool rd_i_flagsbuffer_set_imported(RDFlagsBuffer* self, usize idx) {
    if(idx < self->base.length) {
        rd_i_flags_set_imported(&self->data[idx]);
        return true;
    }

    return false;
}

bool rd_i_flagsbuffer_set_exported(RDFlagsBuffer* self, usize idx) {
    if(idx < self->base.length) {
        rd_i_flags_set_exported(&self->data[idx]);
        return true;
    }

    return false;
}

void rd_i_flagsbuffer_expand_range(const RDFlagsBuffer* self, usize* start,
                                   usize* end) {
    while(*start > 0 && rd_i_flags_has_tail(self->data[*start]))
        (*start)--;

    while(*end < self->base.length && rd_i_flags_has_tail(self->data[*end]))
        (*end)++;
}

RDFlags rd_i_flagsbuffer_get(const RDFlagsBuffer* self, usize idx) {
    assert(idx < self->base.length);
    return self->data[idx];
}

usize rd_i_flagsbuffer_get_range_length(const RDFlagsBuffer* self, usize idx) {
    usize start = idx, end = idx + 1;
    rd_i_flagsbuffer_expand_range(self, &start, &end);
    return end - start;
}

void rd_i_flagsbuffer_undefine(RDFlagsBuffer* self, usize startidx, usize n) {
    usize endidx = startidx + n;
    if(endidx > self->base.length) endidx = self->base.length;

    rd_i_flagsbuffer_expand_range(self, &startidx, &endidx);

    for(usize idx = startidx; idx < endidx; idx++)
        rd_i_flags_undefine(&self->data[idx]);
}

bool rd_i_flagsbuffer_undefine_name(RDFlagsBuffer* self, usize idx) {
    if(idx < self->base.length) {
        rd_i_flags_undefine_name(&self->data[idx]);
        return true;
    }

    return false;
}

bool rd_i_flagsbuffer_undefine_comment(RDFlagsBuffer* self, usize idx) {
    if(idx < self->base.length) {
        rd_i_flags_undefine_comment(&self->data[idx]);
        return true;
    }

    return false;
}
