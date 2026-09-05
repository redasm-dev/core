#pragma once

#include <redasm/support/hash.h>

typedef struct RDHashOps {
    usize digest_length;
    void (*init)(RDHash* self);
    void (*update)(RDHash* self, const void* data, usize n);
    const u8* (*final)(RDHash* self);
} RDHashOps;

typedef struct RDHash {
    RDHashKind kind;
    const RDHashOps* ops;
    bool finalized;

    union {
        struct {
            u32 state[5];
            u64 count;
            u8 buffer[64];
            u8 digest[RD_HASH_SHA1_LENGTH];
        } sha1;

        struct {
            u32 a;
            u32 b;
            u8 digest[RD_HASH_ADLER32_LENGTH];
        } adler32;
    };
} RDHash;
