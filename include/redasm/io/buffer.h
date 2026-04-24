#pragma once

#include <redasm/config.h>

typedef struct RDFlagsBuffer RDFlagsBuffer;

RD_API usize rd_flagsbuffer_get_length(const RDFlagsBuffer* self);

RD_API bool rd_flagsbuffer_get_value(const RDFlagsBuffer* self, usize idx,
                                     u8* v);
