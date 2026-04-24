#pragma once

#include <redasm/function.h>

typedef struct RDFunction {
    RDContext* context;
    RDAddress address;
    RDGraph* graph;
} RDFunction;

typedef struct RDFunctionChunkVect {
    RDFunctionChunk** data;
    usize length;
    usize capacity;
} RDFunctionChunkVect;

RDFunction* rd_i_function_create(RDContext* ctx, RDAddress address);
void rd_i_function_destroy(RDFunction* self);
void rd_i_function_build_graph(RDFunction* self, RDFunctionChunkVect* chunks);
RDFunctionChunk* rd_i_function_get_chunk(const RDFunction* self, RDGraphNode n);

void rd_i_functionchunk_sort(RDFunctionChunkVect* self);
void rd_i_functionchunk_destroy(RDFunctionChunkVect* self);

int rd_i_function_find_chunk_pred(const void* key, const void* item);
int rd_i_function_find_pred(const void* key, const void* item);
