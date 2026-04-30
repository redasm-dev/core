#pragma once

#include <redasm/context.h>

typedef enum {
    RD_CONFIDENCE_AUTO = 0,
    RD_CONFIDENCE_LIBRARY,
    RD_CONFIDENCE_USER,
} RDConfidence;

typedef struct RDName {
    const char* value;
    RDConfidence confidence;
} RDName;

typedef struct RDRegister {
    u64 value;
    RDConfidence confidence;
} RDRegister;
