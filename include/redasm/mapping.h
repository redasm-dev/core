#pragma once

#include <redasm/config.h>

typedef struct RDInputMapping RDInputMapping;

typedef struct RDInputMappingSlice {
    const RDInputMapping** data;
    usize length;
} RDInputMappingSlice;

RD_API RDOffset rd_inputmapping_get_offset(const RDInputMapping* self);
RD_API RDAddress rd_inputmapping_get_start(const RDInputMapping* self);
RD_API RDAddress rd_inputmapping_get_end(const RDInputMapping* self);
RD_API usize rd_inputmapping_get_size(const RDInputMapping* self);
