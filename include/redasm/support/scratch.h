#pragma once

#include <redasm/config.h>

typedef struct RDScratchBuffer RDScratchBuffer;

// clang-format off
RD_API RDScratchBuffer* rd_scratch_create(void);
RD_API void rd_scratch_destroy(RDScratchBuffer* self);
RD_API void rd_scratch_reserve(RDScratchBuffer* self, usize n);
RD_API bool rd_scratch_write(RDScratchBuffer* self, usize idx, const void* data, usize n);
RD_API void rd_scratch_append(RDScratchBuffer* self, const void* data, usize n);
RD_API void rd_scratch_push(RDScratchBuffer* self, char c);
RD_API bool rd_scratch_get(RDScratchBuffer* self, usize idx, char* c);
RD_API bool rd_scratch_set(RDScratchBuffer* self, usize idx, char c);
RD_API void rd_scratch_clear(RDScratchBuffer* self);
RD_API void rd_scratch_puts(RDScratchBuffer* self, const char* s);
RD_API void rd_scratch_puts_n(RDScratchBuffer* self, const char* s, usize n);
RD_API const char* rd_scratch_data(const RDScratchBuffer* self);
RD_API usize rd_scratch_length(const RDScratchBuffer* self);
RD_API usize rd_scratch_capacity(const RDScratchBuffer* self);
// clang-format on
