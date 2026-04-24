#pragma once

#include "core/function.h"
#include "core/segment.h"
#include "types/def.h"
#include <redasm/listing.h>

#define RD_LISTING_HEX_LINE 0x10
#define RD_LISTING_NO_INDEX ((usize) - 1)

typedef struct RDFunction RDFunction;
typedef usize LIndex;

typedef struct RDAddressVect {
    RDAddress* data;
    usize length;
    usize capacity;
} RDAddressVect;

typedef struct RDSymbolVect {
    RDSymbol* data;
    usize length;
    usize capacity;
} RDSymbolVect;

typedef enum {
    RD_LK_EMPTY = 0,
    RD_LK_HEX_DUMP,
    RD_LK_FILL,
    RD_LK_INSTRUCTION,
    RD_LK_LABEL,
    RD_LK_SEGMENT,
    RD_LK_FUNCTION,
    RD_LK_TYPE,
} RDListingItemKind;

typedef enum {
    RD_LMOD_NONE = 0,
    RD_LMOD_EXPORTED,
    RD_LMOD_IMPORTED,
} RDListingModifier;

#define RD_LISTING_STACK_SIZE 256

typedef struct RDListingFillByte {
    u8 value;
    bool has_value;
} RDListingFillByte;

typedef struct RDListingItem {
    RDListingItemKind kind;
    RDListingModifier mod;
    int indent;

    union {
        RDAddress start_address;
        RDAddress address;
    };

    RDAddress end_address; // also used as length for RD_LK_HEX_DUMP

    union {
        struct { // RD_LK_TYPE
            RDType type;
            const RDTypeDef* parent_tdef;
            usize array_index;  // RD_LISTING_NO_INDEX = not set
            usize member_index; // RD_LISTING_NO_INDEX = not set
        };
        const RDFunction* func; // RD_LK_FUNCTION
        RDListingFillByte fill; // RD_LK_FILL
    };
} RDListingItem;

typedef struct RDListing {
    RDListingItem* data;
    usize length;
    usize capacity;
    int indent;

    RDSymbolVect symbols;
    RDAddressVect exported;
    RDAddressVect imported;
    RDFunctionChunkVect chunks;

    struct {
        RDFunction** data;
        usize length;
        usize capacity;
    } functions;
} RDListing;

void rd_i_listing_init(RDListing* self, RDListing* prev);
void rd_i_listing_deinit(RDListing* self);
void rd_i_listing_push_indent(RDListing* self, int c);
void rd_i_listing_pop_indent(RDListing* self, int c);
LIndex rd_i_listing_lower_bound(const RDListing* self, RDAddress address);
LIndex rd_i_listing_add_segment(RDListing* self, const RDSegmentFull* s);
LIndex rd_i_listing_add_function(RDListing* self, const RDFunction* f);
LIndex rd_i_listing_add_instruction(RDListing* self, RDAddress addr);
LIndex rd_i_listing_add_label(RDListing* self, RDAddress addr);
LIndex rd_i_listing_add_type(RDListing* self, RDAddress address,
                             const RDType* t);
LIndex rd_i_listing_add_hex_dump(RDListing* self, RDAddress startaddr,
                                 RDAddress endaddr);
LIndex rd_i_listing_add_fill(RDListing* self, RDAddress startaddr,
                             RDAddress endaddr, RDListingFillByte fb);
