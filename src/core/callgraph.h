#pragma once

#include "db/types.h"
#include "function/chunk.h"
#include "graphing/graph.h"
#include <redasm/callgraph.h>

typedef struct RDCallVect {
    RDCall* data;
    usize length;
    usize capacity;
} RDCallVect;

typedef struct RDCallGraph {
    RDGraph base;
    RDXRefVect xrefs_buf;
    RDChunkVect chunks_buf;
    RDCharVect fmt_buf;
    RDCallVect calls_buf;

    struct {
        RDGraphNode* data;
        usize length;
        usize capacity;
    } wl;
} RDCallGraph;

void rd_i_callgraph_collect_callers(RDContext* ctx, RDRelAddress rel_entry,
                                    RDXRefVect* xrefs_buf, RDCallVect* out);
void rd_i_callgraph_collect_callees(RDContext* ctx, RDRelAddress rel_entry,
                                    RDChunkVect* chunks_buf,
                                    RDXRefVect* xrefs_buf, RDCallVect* out);
