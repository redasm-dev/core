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

typedef struct RDRegValueFull {
    RDRegValue value;
    RDConfidence confidence;
} RDRegValueFull;

typedef struct RDTrackedRegVect {
    RDTrackedReg* data;
    usize length;
    usize capacity;
} RDTrackedRegVect;
