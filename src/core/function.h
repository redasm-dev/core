#pragma once

#include <redasm/function.h>

typedef struct RDFunction {
    RDContext* context;
    RDAddress address;
    usize n_instructions;
    usize n_norets;
    RDGraph* graph;
} RDFunction;

typedef struct RDFunctionChunkVect {
    RDFunctionChunk** data;
    usize length;
    usize capacity;
} RDFunctionChunkVect;

typedef struct RDFunctionVect {
    RDFunctionChunkVect chunks;

    RDFunction** data;
    usize length;
    usize capacity;
} RDFunctionVect;

RDFunction* rd_i_function_create(RDContext* ctx, RDAddress address);
void rd_i_function_destroy(RDFunction* self);
bool rd_i_function_is_noret(const RDFunction* self);
usize rd_i_function_get_terminal_count(const RDFunction* self);
const char* rd_i_function_to_str(const RDFunction* self, RDContext* ctx);
RDFunctionChunk* rd_i_function_get_chunk(const RDFunction* self, RDGraphNode n);

void rd_i_functionchunk_sort(RDFunctionChunkVect* self);
void rd_i_functionchunk_destroy(RDFunctionChunkVect* self);

void rd_i_rebuild_all_functions(RDContext* ctx);
void rd_i_rebuild_function(RDFunction* f);
void rd_i_functionvect_destroy(RDFunctionVect* self);

int rd_i_functionchunk_kcmp_pred(const void* key, const void* item);
int rd_i_function_kcmp_pred(const void* key, const void* item);
