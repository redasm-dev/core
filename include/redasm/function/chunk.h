#pragma once

#include <redasm/common.h>

typedef struct RDFunctionChunk RDFunctionChunk;

// clang-format off
RD_API RDAddress rd_functionchunk_get_start(const RDFunctionChunk* self);
RD_API RDAddress rd_functionchunk_get_end(const RDFunctionChunk* self);
RD_API usize rd_functionchunk_get_instruction_count(const RDFunctionChunk* self);
RD_API bool rd_functionchunk_has_noret(const RDFunctionChunk* self);
// clang-format on
