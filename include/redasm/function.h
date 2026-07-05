#pragma once

#include <redasm/config.h>
#include <redasm/graph/graph.h>

typedef struct RDFunction RDFunction;

typedef struct RDFunctionSlice {
    const RDFunction** data;
    usize length;
} RDFunctionSlice;

typedef struct RDFunctionChunk {
    const RDFunction* func;
    RDAddress start;
    RDAddress end;
    usize n_instructions;
    bool has_noret;
} RDFunctionChunk;

RD_API RDGraph* rd_function_get_graph(const RDFunction* self);
RD_API RDAddress rd_function_get_address(const RDFunction* self);
RD_API usize rd_function_get_n_instructions(const RDFunction* self);
RD_API const char* rd_function_generate_dot(const RDFunction* self);
RD_API u32 rd_function_get_hash(const RDFunction* self);
RD_API bool rd_function_is_noret(const RDFunction* self);
RD_API bool rd_function_contains_address(const RDFunction* self,
                                         RDAddress address);
RD_API const RDFunctionChunk* rd_function_get_chunk(const RDFunction* self,
                                                    RDGraphNode n);
