#pragma once

#include <redasm/config.h>
#include <redasm/graph/graph.h>
#include <redasm/types/def.h>

typedef struct RDFunction RDFunction;
typedef struct RDFunctionChunk RDFunctionChunk;

typedef struct RDFunctionSlice {
    const RDFunction** data;
    usize length;
} RDFunctionSlice;

// clang-format off
RD_API bool rd_function_is_same(const RDFunction* self, const RDFunction* f);
RD_API RDGraph* rd_function_get_graph(const RDFunction* self);
RD_API const RDTypeDef* rd_function_get_type(const RDFunction* self);
RD_API RDAddress rd_function_get_address(const RDFunction* self);
RD_API usize rd_function_get_n_instructions(const RDFunction* self);
RD_API const char* rd_function_generate_dot(const RDFunction* self);
RD_API u32 rd_function_get_hash(const RDFunction* self);
RD_API const char* rd_function_generate_dot_layout(const RDFunction* self);
RD_API u32 rd_function_get_hash_layout(const RDFunction* self);
RD_API bool rd_function_is_noret(const RDFunction* self);
RD_API bool rd_function_contains_address(const RDFunction* self, RDAddress address);
RD_API const RDFunctionChunk* rd_function_get_chunk(const RDFunction* self, RDGraphNode n);

RD_API RDAddress rd_functionchunk_get_start(const RDFunctionChunk* self);
RD_API RDAddress rd_functionchunk_get_end(const RDFunctionChunk* self);
RD_API usize rd_functionchunk_get_instruction_count(const RDFunctionChunk* self);
RD_API bool rd_functionchunk_has_noret(const RDFunctionChunk* self);
// clang-format on
