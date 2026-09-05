#pragma once

#include <redasm/config.h>

#define RD_HASH_SHA1_LENGTH 20
#define RD_HASH_ADLER32_LENGTH 4

typedef struct RDHash RDHash;

typedef enum {
    RD_HASH_SHA1,
    RD_HASH_ADLER32,
} RDHashKind;

RD_API RDHash* rd_hash_create(RDHashKind kind);
RD_API void rd_hash_destroy(RDHash* self);
RD_API void rd_hash_update(RDHash* self, const char* data, usize n);
RD_API void rd_hash_reset(RDHash* self);
RD_API const u8* rd_hash_final(RDHash* self);
RD_API bool rd_hash_final_to(RDHash* self, void* dst, usize cap);
RD_API usize rd_hash_get_length(const RDHash* self);
