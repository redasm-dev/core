#pragma once

#include "core/segment.h"
#include "function/chunk.h"
#include "support/utils.h"
#include "types/def.h"
#include <redasm/function/function.h>

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

typedef struct RDFunctionVect {
    RDFunctionChunkVect chunks;

    RDFunction** data;
    usize length;
    usize capacity;
} RDFunctionVect;

int rd_i_function_kcmp_pred(const void* key, const void* item);
bool rd_i_function_declare_if(RDContext* ctx, const RDSegment* seg, usize idx,
                              const char* type);
RDFunction* rd_i_function_declare(RDContext* ctx, RDRelAddress address,
                                  const char* type);
void rd_i_function_undeclare(RDContext* ctx, const RDSegment* seg, usize idx);

void rd_i_function_set_type_def(RDFunction* self, const RDTypeDef* tdef);
void rd_i_function_rebuild(RDFunction* self);
void rd_i_function_rebuild_graph(RDFunction* self, RDFunctionChunkVect* chunks);
RDFunctionChunk* rd_i_function_get_chunk(const RDFunction* self, RDGraphNode n);

void rd_i_functionvect_destroy(RDFunctionVect* self);
