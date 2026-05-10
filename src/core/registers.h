#pragma once

#include "support/containers.h"
#include <redasm/registers.h>

typedef struct RDRegister {
    HMapHeader hmap_hdr;

    const char* name;
    RDRegValue value;
} RDRegister;

typedef struct RDRegisterHMap {
    RDRegister* data;
    usize length;
    usize capacity;

    HMapHash hash;
    HMapEqual equal;
} RDRegisterHMap;

void rd_i_registermap_init(RDRegisterHMap* self);
