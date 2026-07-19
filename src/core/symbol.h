#pragma once

#include <redasm/symbol.h>

typedef struct RDSymbolVect {
    RDSymbol* data;
    usize length;
    usize capacity;
} RDSymbolVect;

int rd_i_symbol_sort_pred(const void* a, const void* b);
