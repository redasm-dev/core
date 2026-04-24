#pragma once

#include "io/flagsbuffer.h"
#include <redasm/config.h>

typedef struct RDPatternByte {
    bool wildcard;
    u8 value;
} RDPatternByte;

typedef struct RDPattern {
    RDPatternByte* data;
    usize length;
    usize capacity;
} RDPattern;

bool rd_pattern_compile(RDPattern* self, const char* s);
void rd_pattern_destroy(RDPattern* self);
bool rd_pattern_match(const RDPattern* self, const RDFlagsBuffer* flags,
                      usize idx);
