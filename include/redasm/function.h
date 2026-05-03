#pragma once

#include <redasm/config.h>
#include <redasm/graph/graph.h>

typedef struct RDFunction RDFunction;

typedef struct RDFunctionSlice {
    const RDFunction** data;
    usize length;
} RDFunctionSlice;

typedef struct RDFunctionChunk {
    RDAddress func_address;
    RDAddress start;
    RDAddress end;
} RDFunctionChunk;

RD_API RDGraph* rd_function_get_graph(const RDFunction* self);
RD_API RDAddress rd_function_get_address(const RDFunction* self);
RD_API usize rd_function_get_n_instructions(const RDFunction* self);
RD_API bool rd_function_get_chunk(const RDFunction* self, RDGraphNode n,
                                  RDFunctionChunk* chunk);
