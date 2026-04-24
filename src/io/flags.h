#pragma once

#include <redasm/redasm.h>

bool rd_i_flags_has_unknown(RDFlags self);
bool rd_i_flags_has_info(RDFlags self);
bool rd_i_flags_has_code(RDFlags self);
bool rd_i_flags_has_data(RDFlags self);
bool rd_i_flags_has_tail(RDFlags self);
bool rd_i_flags_has_name(RDFlags self);
bool rd_i_flags_has_comment(RDFlags self);
bool rd_i_flags_has_flow(RDFlags self);
bool rd_i_flags_has_jump(RDFlags self);
bool rd_i_flags_has_jmpdst(RDFlags self);
bool rd_i_flags_has_call(RDFlags self);
bool rd_i_flags_has_func(RDFlags self);
bool rd_i_flags_has_noret(RDFlags self);
bool rd_i_flags_has_cond(RDFlags self);
bool rd_i_flags_has_type(RDFlags self);
bool rd_i_flags_has_xref_out(RDFlags self);
bool rd_i_flags_has_xref_in(RDFlags self);
bool rd_i_flags_has_imported(RDFlags self);
bool rd_i_flags_has_exported(RDFlags self);
bool rd_i_flags_get_value(RDFlags self, u8* b);
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
void rd_i_flags_set_type(RDFlags* self);
void rd_i_flags_set_name(RDFlags* self);
void rd_i_flags_set_comment(RDFlags* self);
void rd_i_flags_set_xref_out(RDFlags* self);
void rd_i_flags_set_xref_in(RDFlags* self);
void rd_i_flags_set_imported(RDFlags* self);
void rd_i_flags_set_exported(RDFlags* self);
void rd_i_flags_undefine(RDFlags* self);
void rd_i_flags_undefine_name(RDFlags* self);
void rd_i_flags_undefine_comment(RDFlags* self);
