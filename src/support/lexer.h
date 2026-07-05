#pragma once

#include "support/utils.h"
#include <redasm/support/lexer.h>

typedef struct RDLexer {
    const char* input;
    const char* curr;

    int base;

    usize line;
    usize col;
    usize pos;

    RDCharVect token_value_buf;
} RDLexer;
