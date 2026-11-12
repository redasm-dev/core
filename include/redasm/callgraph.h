#pragma once

#include <redasm/common.h>
#include <redasm/config.h>
#include <redasm/context.h>
#include <redasm/graphing/graph.h>

typedef struct RDCall {
    RDAddress address; // the call instruction
    RDAddress target;  // what it calls
    RDXRefType type; // RD_CR_CALL for a direct call, anything else for indirect
} RDCall;

typedef struct RDCallSlice {
    const RDCall* data;
    usize length;
} RDCallSlice;

typedef struct RDCallGraph RDCallGraph;

RD_API RDCallGraph* rd_callgraph_create(RDContext* ctx, RDAddress address);
RD_API void rd_callgraph_destroy(RDCallGraph* self);
RD_API RDAddress rd_callgraph_get_address(const RDCallGraph* self,
                                          RDGraphNode n);
RD_API u32 rd_callgraph_get_hash(const RDCallGraph* self);
RD_API const char* rd_callgraph_generate_dot(const RDCallGraph* self);

// 'address' can be anywhere inside the function. Sorted by call site.
RD_API RDCallSlice rd_get_callees(RDContext* self, RDAddress address);
RD_API RDCallSlice rd_get_callers(RDContext* self, RDAddress address);
