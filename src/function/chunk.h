#pragma once

#include <redasm/function/chunk.h>
#include <redasm/function/function.h>

typedef struct RDFunctionChunk {
    const RDFunction* func;
    RDRelAddress start;
    RDRelAddress end; // exclusive
    usize n_instructions;
    bool has_noret;
} RDFunctionChunk;

typedef struct RDFunctionChunkVect {
    RDFunctionChunk** data;
    usize length;
    usize capacity;
} RDFunctionChunkVect;

void rd_i_functionchunk_sort(RDFunctionChunkVect* self);
void rd_i_functionchunk_destroy(RDFunctionChunkVect* self);
int rd_i_functionchunk_kcmp_pred(const void* key, const void* item);
