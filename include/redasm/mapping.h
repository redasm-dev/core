#pragma once

#include <redasm/config.h>

typedef struct RDInputMapping {
    RDOffset offset;
    RDAddress start_address;
    RDAddress end_address;
} RDInputMapping;

typedef struct RDInputMappingSlice {
    const RDInputMapping* data;
    usize length;
} RDInputMappingSlice;
