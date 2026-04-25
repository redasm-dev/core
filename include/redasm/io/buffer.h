#pragma once

#include <redasm/config.h>

typedef struct RDFlagsBuffer RDFlagsBuffer;

RD_API usize rd_flagsbuffer_get_length(const RDFlagsBuffer* self);
RD_API bool rd_flagsbuffer_has_unknown(const RDFlagsBuffer* self, usize idx);
RD_API bool rd_flagsbuffer_has_code(const RDFlagsBuffer* self, usize idx);
RD_API bool rd_flagsbuffer_has_data(const RDFlagsBuffer* self, usize idx);
RD_API bool rd_flagsbuffer_has_tail(const RDFlagsBuffer* self, usize idx);
RD_API bool rd_flagsbuffer_has_flow(const RDFlagsBuffer* self, usize idx);
RD_API bool rd_flagsbuffer_has_func(const RDFlagsBuffer* self, usize idx);
RD_API bool rd_flagsbuffer_has_noret(const RDFlagsBuffer* self, usize idx);
RD_API bool rd_flagsbuffer_has_jump(const RDFlagsBuffer* self, usize idx);
RD_API bool rd_flagsbuffer_has_call(const RDFlagsBuffer* self, usize idx);
RD_API bool rd_flagsbuffer_has_cond(const RDFlagsBuffer* self, usize idx);
RD_API bool rd_flagsbuffer_has_name(const RDFlagsBuffer* self, usize idx);
RD_API bool rd_flagsbuffer_has_imported(const RDFlagsBuffer* self, usize idx);
RD_API bool rd_flagsbuffer_has_exported(const RDFlagsBuffer* self, usize idx);
RD_API bool rd_flagsbuffer_get_value(const RDFlagsBuffer* self, usize idx,
                                     u8* v);
