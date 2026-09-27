#pragma once

#include <redasm/common.h>
#include <redasm/config.h>
#include <redasm/graph/graph.h>

typedef struct RDCallGraph RDCallGraph;

RD_API RDCallGraph* rd_callgraph_create(RDContext* ctx, RDAddress address);
RD_API void rd_callgraph_destroy(RDCallGraph* self);
RD_API RDAddress rd_callgraph_get_address(const RDCallGraph* self,
                                          RDGraphNode n);
