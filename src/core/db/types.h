#pragma once

#include <redasm/context.h>
#include <redasm/registers.h>

typedef enum {
    RD_CONFIDENCE_AUTO = 0,
    RD_CONFIDENCE_LIBRARY,
    RD_CONFIDENCE_USER,
} RDConfidence;

typedef struct RDName {
    const char* value;
    RDConfidence confidence;
} RDName;

typedef struct RDRegisterValue {
    u64 value;
    RDConfidence confidence;
} RDRegisterValue;

typedef struct RDTrackedRegisterVect {
    RDTrackedRegister* data;
    usize length;
    usize capacity;
} RDTrackedRegisterVect;
