#pragma once

#include <redasm/io/flags.h>

bool rd_i_flags_has_info(RDFlags self);
bool rd_i_flags_has_flow(RDFlags self);
bool rd_i_flags_has_jmpdst(RDFlags self);
bool rd_i_flags_has_dslot(RDFlags self);
bool rd_i_flags_has_op_over(RDFlags self);
bool rd_i_flags_has_field(RDFlags self);
bool rd_i_flags_has_item(RDFlags self);
void rd_i_flags_set_value(RDFlags* self, u8 v);
void rd_i_flags_set_code(RDFlags* self);
void rd_i_flags_set_data(RDFlags* self);
void rd_i_flags_set_tail(RDFlags* self);
void rd_i_flags_set_flow(RDFlags* self);
void rd_i_flags_set_jump(RDFlags* self);
void rd_i_flags_set_jmpdst(RDFlags* self);
void rd_i_flags_set_call(RDFlags* self);
void rd_i_flags_set_func(RDFlags* self);
void rd_i_flags_set_noret(RDFlags* self);
void rd_i_flags_set_cond(RDFlags* self);
void rd_i_flags_set_dslot(RDFlags* self);
void rd_i_flags_set_op_over(RDFlags* self);
void rd_i_flags_set_type(RDFlags* self);
void rd_i_flags_set_field(RDFlags* self);
void rd_i_flags_set_item(RDFlags* self);
void rd_i_flags_set_name(RDFlags* self);
void rd_i_flags_set_patch(RDFlags* self);
void rd_i_flags_set_comment(RDFlags* self);
void rd_i_flags_set_xref_out(RDFlags* self);
void rd_i_flags_set_xref_in(RDFlags* self);
void rd_i_flags_set_imported(RDFlags* self);
void rd_i_flags_set_exported(RDFlags* self);
void rd_i_flags_undefine(RDFlags* self);
void rd_i_flags_clear(RDFlags* self);
void rd_i_flags_clear_name(RDFlags* self);
void rd_i_flags_clear_patch(RDFlags* self);
void rd_i_flags_clear_func(RDFlags* self);
void rd_i_flags_clear_comment(RDFlags* self);
void rd_i_flags_clear_xref_out(RDFlags* self);
void rd_i_flags_clear_xref_in(RDFlags* self);
void rd_i_flags_clear_flow(RDFlags* self);
void rd_i_flags_clear_op_over(RDFlags* self);
