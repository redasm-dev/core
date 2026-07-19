#pragma once

#include "surface/renderer.h"

typedef struct RDRenderItemResult {
    RDRowStatus status;
    usize length;
} RDRenderItemResult;

typedef struct RDDataHead {
    RDType root;     // the FL_TYPE root governing this address
    usize offset;    // this head's byte offset from the root's address
    bool has_banner; // FL_TYPE heads render the root itself as an extra row
} RDDataHead;

bool rd_i_render_segment_item(RDRenderer* r, const RDSegmentFull* seg);
void rd_i_data_head_get(RDContext* ctx, const RDSegmentFull* seg, usize idx,
                        RDDataHead* out);
bool rd_i_data_chain_row(RDContext* ctx, const RDDataHead* head, usize link,
                         RDResolveResult* out);

RDRenderItemResult rd_i_render_item(RDRenderer* r, const RDSegmentFull* seg,
                                    usize idx, usize sub_line);
