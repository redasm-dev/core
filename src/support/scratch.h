#pragma once

#include "support/utils.h"
#include <redasm/support/scratch.h>

typedef struct RDScratchBuffer {
    RDCharVect impl;
    RDCharVect fmt_buf;
} RDScratchBuffer;
