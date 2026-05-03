#pragma once

#include "io/buffer.h"

typedef struct RDFlagsBuffer {
    RDBuffer base;
    RDFlags* data;
} RDFlagsBuffer;

RDFlagsBuffer* rd_i_flagsbuffer_create(usize n);
bool rd_i_flagsbuffer_has_unknown_n(const RDFlagsBuffer* self, usize startidx,
                                    usize n);
bool rd_i_flagsbuffer_has_code_n(const RDFlagsBuffer* self, usize startidx,
                                 usize n);
bool rd_i_flagsbuffer_has_data_n(const RDFlagsBuffer* self, usize startidx,
                                 usize n);
bool rd_i_flagsbuffer_has_info(const RDFlagsBuffer* self, usize idx);
bool rd_i_flagsbuffer_has_comment(const RDFlagsBuffer* self, usize idx);
bool rd_i_flagsbuffer_has_xref_out(const RDFlagsBuffer* self, usize idx);
bool rd_i_flagsbuffer_has_xref_in(const RDFlagsBuffer* self, usize idx);
bool rd_i_flagsbuffer_has_op_over(const RDFlagsBuffer* self, usize idx);

bool rd_i_flagsbuffer_has_type(const RDFlagsBuffer* self, usize idx);
bool rd_i_flagsbuffer_has_jmpdst(const RDFlagsBuffer* self, usize idx);

void rd_i_flagsbuffer_expand_range(const RDFlagsBuffer* self, usize* start,
                                   usize* end);
RDFlags rd_i_flagsbuffer_get(const RDFlagsBuffer* self, usize idx);
usize rd_i_flagsbuffer_get_range_length(const RDFlagsBuffer* self, usize idx);

bool rd_i_flagsbuffer_set_value(RDFlagsBuffer* self, usize idx, u8 v);
bool rd_i_flagsbuffer_set_data(RDFlagsBuffer* self, usize idx, usize n);
bool rd_i_flagsbuffer_set_code(RDFlagsBuffer* self, usize idx, usize n);
bool rd_i_flagsbuffer_set_flow(RDFlagsBuffer* self, usize idx);
bool rd_i_flagsbuffer_set_jump(RDFlagsBuffer* self, usize idx);
bool rd_i_flagsbuffer_set_jmpdst(RDFlagsBuffer* self, usize idx);
bool rd_i_flagsbuffer_set_call(RDFlagsBuffer* self, usize idx);
bool rd_i_flagsbuffer_set_func(RDFlagsBuffer* self, usize idx);
bool rd_i_flagsbuffer_set_noret(RDFlagsBuffer* self, usize idx);
bool rd_i_flagsbuffer_set_cond(RDFlagsBuffer* self, usize idx);
bool rd_i_flagsbuffer_set_dslot(RDFlagsBuffer* self, usize idx);
bool rd_i_flagsbuffer_set_op_over(RDFlagsBuffer* self, usize idx);
bool rd_i_flagsbuffer_set_type(RDFlagsBuffer* self, usize idx, usize n);
bool rd_i_flagsbuffer_set_name(RDFlagsBuffer* self, usize idx);
bool rd_i_flagsbuffer_set_comment(RDFlagsBuffer* self, usize idx);
bool rd_i_flagsbuffer_set_xref_out(RDFlagsBuffer* self, usize idx);
bool rd_i_flagsbuffer_set_xref_in(RDFlagsBuffer* self, usize idx);
bool rd_i_flagsbuffer_set_imported(RDFlagsBuffer* self, usize idx);
bool rd_i_flagsbuffer_set_exported(RDFlagsBuffer* self, usize idx);

void rd_i_flagsbuffer_undefine(RDFlagsBuffer* self, usize startidx, usize n);
bool rd_i_flagsbuffer_undefine_name(RDFlagsBuffer* self, usize idx);
bool rd_i_flagsbuffer_undefine_comment(RDFlagsBuffer* self, usize idx);
bool rd_i_flagsbuffer_undefine_xref_out(RDFlagsBuffer* self, usize idx);
bool rd_i_flagsbuffer_undefine_xref_in(RDFlagsBuffer* self, usize idx);
bool rd_i_flagsbuffer_undefine_op_over(RDFlagsBuffer* self, usize idx);
