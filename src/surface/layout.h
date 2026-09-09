#pragma once

#include "core/segment.h"
#include <redasm/surface/common.h>
#include <redasm/types/type.h>

#define RD_SURFACE_HEX_LINE 0x10

typedef enum RDRowKind {
    RD_ROWKIND_NONE = 0,
    RD_ROWKIND_SEGMENT,
    RD_ROWKIND_COMMENT_BEFORE,
    RD_ROWKIND_FUNCTION,
    RD_ROWKIND_LABEL,
    RD_ROWKIND_INSTRUCTION,
    RD_ROWKIND_NORET,
    RD_ROWKIND_HEXDUMP,
    RD_ROWKIND_DATA_BANNER, // the FL_TYPE root, rendered as its own row
    RD_ROWKIND_DATA_LINK,   // one entity of the coincidence chain
    RD_ROWKIND_DATA_ELEMENTS,
    RD_ROWKIND_COMMENT_AFTER,
} RDRowKind;

// Every descriptor produces exactly one row.
// A body that renders nothing must not have had a descriptor pushed, flags that
// suppress a row belong in layout, not in the body.
typedef struct RDRowDesc {
    RDRowKind kind;
    usize length;
    usize indent;

    union {
        usize comment_idx;
        RDResolveResult resolve;

        struct {
            RDType type;
            usize count;
        } elements;
    };
} RDRowDesc;

typedef struct RDRowDescVect {
    RDRowDesc* data;
    usize length;
    usize capacity;
} RDRowDescVect;

typedef struct RDDataHead {
    RDType root;
    usize offset;
    bool has_banner;
} RDDataHead;

usize rd_i_item_layout(RDContext* ctx, RDRenderFlags flags,
                       const RDSegmentFull* seg, usize idx, RDRowDescVect* out);
usize rd_i_item_layout_content(const RDRowDescVect* rows);
void rd_i_data_head_get(RDContext* ctx, const RDSegmentFull* seg, usize idx,
                        RDDataHead* out);
bool rd_i_is_hexchunk_head(const RDSegmentFull* seg, usize idx);
bool rd_i_link_is_packable(const RDResolveResult* res);
bool rd_i_is_packed_element_head(const RDSegmentFull* seg, usize idx,
                                 usize elem_size, usize item_idx);
