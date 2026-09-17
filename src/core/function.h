#pragma once

#include "core/segment.h"
#include "support/utils.h"
#include "types/def.h"
#include <redasm/function.h>

typedef struct RDFunction {
    u32 gen;

    const RDTypeDef* type_def;
    RDContext* context;
    RDRelAddress rel_address;
    usize n_instructions;
    usize n_norets;
    RDGraph* graph;
    RDCharVect fmt_buf;
} RDFunction;

typedef struct RDFunctionChunk {
    const RDFunction* func;
    RDRelAddress start;
    RDRelAddress end;
    usize n_instructions;
    bool has_noret;
} RDFunctionChunk;

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

int rd_i_function_kcmp_pred(const void* key, const void* item);
void rd_i_function_declare_if(RDContext* ctx, const RDSegment* seg,
                              usize idx, const char* type);
RDFunction* rd_i_function_declare(RDContext* ctx, RDRelAddress address,
                                  const char* type);
void rd_i_function_undeclare(RDContext* ctx, const RDSegment* seg,
                             usize idx);

void rd_i_function_set_type_def(RDFunction* self, const RDTypeDef* tdef);
void rd_i_function_rebuild(RDFunction* self);
void rd_i_function_rebuild_graph(RDFunction* self, RDFunctionChunkVect* chunks);
RDFunctionChunk* rd_i_function_get_chunk(const RDFunction* self, RDGraphNode n);

void rd_i_functionchunk_sort(RDFunctionChunkVect* self);
void rd_i_functionchunk_destroy(RDFunctionChunkVect* self);

void rd_i_functionvect_destroy(RDFunctionVect* self);

int rd_i_functionchunk_kcmp_pred(const void* key, const void* item);
