#pragma once

#include <redasm/common.h>
#include <redasm/config.h>

typedef enum {
    RD_SYMBOL_INVALID = 0,
    RD_SYMBOL_SEGMENT,
    RD_SYMBOL_FUNCTION,
    RD_SYMBOL_TYPE,
    RD_SYMBOL_STRING,
    RD_SYMBOL_IMPORTED,
    RD_SYMBOL_EXPORTED,
} RDSymbolKind;

typedef struct RDSymbol {
    RDSymbolKind kind;
    RDAddress address;
} RDSymbol;

RD_API const char* rd_symbol_to_string(const RDSymbol* self, RDContext* ctx);
