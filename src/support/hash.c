#include "hash.h"

#define MURMUR3_C1 (u32)(0xcc9e2d51)
#define MURMUR3_C2 (u32)(0x1b873593)
#define MURMUR3_SEED (u32)(0xbc9f1d34)

// clang-format off
static u32 _murmur3_get_block(const char* p, unsigned i) {
    return (u32)(p[0 + (i * 4)]) << 0 |
           (u32)(p[1 + (i * 4)]) << 8 |
           (u32)(p[2 + (i * 4)]) << 16 |
           (u32)(p[3 + (i * 4)]) << 24;
}

static u32 _murmur3_fmix32(u32 h) {
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

static u32 _murmur3_rotl32(u32 x, u8 r) { return (x << r) | (x >> (32U - r)); }
// clang-format on

u32 rd_i_murmur3(const char* s, usize len) {
    const usize N_BLOCKS = len / 4;
    u32 h1 = MURMUR3_SEED;

    for(usize i = 0; i < N_BLOCKS; i++) {
        u32 k1 = _murmur3_get_block(s, i);

        k1 *= MURMUR3_C1;
        k1 = _murmur3_rotl32(k1, 15);
        k1 *= MURMUR3_C2;

        h1 ^= k1;
        h1 = _murmur3_rotl32(h1, 13);
        h1 = (h1 * 5) + 0xe6546b64;
    }

    const usize TAIL = len - (len % 4);
    u32 k1 = 0;

    switch(len & 3) {
        case 3: k1 ^= s[TAIL + 2] << 16; // fall through
        case 2: k1 ^= s[TAIL + 1] << 8;  // fall through

        case 1:
            k1 ^= s[TAIL + 0];
            k1 *= MURMUR3_C1;
            k1 = _murmur3_rotl32(k1, 15);
            k1 *= MURMUR3_C2;
            h1 ^= k1;
            // fall through

        default: break;
    };

    return _murmur3_fmix32(h1 ^ len);
}
