#pragma once

#include "db/types.h"
#include "graphing/graph.h"
#include <redasm/callgraph.h>

typedef struct RDCallGraph {
    RDGraph base;
    RDContext* context;
    RDXRefVect xrefs_buf;

    struct {
        RDGraphNode* data;
        usize length;
        usize capacity;
    } wl;
} RDCallGraph;
