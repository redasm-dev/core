#pragma once

#include <redasm/common.h>
#include <redasm/config.h>

typedef u32 RDFlags;

RD_API bool rd_get_flags(const RDContext* self, RDAddress address, RDFlags* f);

// class
RD_API bool rd_flags_has_unknown(RDFlags self);
RD_API bool rd_flags_has_code(RDFlags self);
RD_API bool rd_flags_has_data(RDFlags self);
RD_API bool rd_flags_has_tail(RDFlags self);

// what's here
RD_API bool rd_flags_has_func(RDFlags self);
RD_API bool rd_flags_has_name(RDFlags self);
RD_API bool rd_flags_has_type(RDFlags self);
RD_API bool rd_flags_has_comment(RDFlags self);
RD_API bool rd_flags_has_patch(RDFlags self);
RD_API bool rd_flags_has_xref_in(RDFlags self);
RD_API bool rd_flags_has_xref_out(RDFlags self);
RD_API bool rd_flags_has_imported(RDFlags self);
RD_API bool rd_flags_has_exported(RDFlags self);

// instruction kind (CODE only)
RD_API bool rd_flags_has_jump(RDFlags self);
RD_API bool rd_flags_has_call(RDFlags self);
RD_API bool rd_flags_has_cond(RDFlags self);
RD_API bool rd_flags_has_noret(RDFlags self);

// byte value
RD_API bool rd_flags_get_value(RDFlags self, u8* b);
