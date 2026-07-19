#pragma once

#include <redasm/common.h>
#include <redasm/config.h>
#include <redasm/function.h>
#include <redasm/segment.h>
#include <redasm/types/type.h>

typedef enum {
    RD_SYMBOL_INVALID = 0,
    RD_SYMBOL_SEGMENT,
    RD_SYMBOL_FUNCTION,
    RD_SYMBOL_TYPE,
    RD_SYMBOL_NAME,
} RDSymbolKind;

typedef struct RDSymbol {
    RDSymbolKind kind;
    RDAddress address;

    union {
        const RDSegment* segment;
        const RDFunction* func;
        RDType type;
    };
} RDSymbol;

typedef struct RDSymbolSlice {
    const RDSymbol* data;
    usize length;
} RDSymbolSlice;

RD_API const char* rd_symbol_to_string(const RDSymbol* self, RDContext* ctx);
