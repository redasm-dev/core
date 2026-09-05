#include "sha1.h"
#include <redasm/support/byteorder.h>
#include <stddef.h>
#include <string.h>

#define _sha1_rol(value, bits)                                                 \
    (((value) << (bits)) | ((value) >> (32 - (bits))))

#define _sha1_blk0(i) (l[i])

#define _sha1_blk(i)                                                           \
    (l[(i) & 15] = _sha1_rol(l[((i) + 13) & 15] ^ l[((i) + 8) & 15] ^          \
                                 l[((i) + 2) & 15] ^ l[(i) & 15],              \
                             1))

#define _sha1_R0(v, w, x, y, z, i)                                             \
    z += ((w & (x ^ y)) ^ y) + _sha1_blk0(i) + 0x5A827999 + _sha1_rol(v, 5);   \
    w = _sha1_rol(w, 30);
#define _sha1_R1(v, w, x, y, z, i)                                             \
    z += ((w & (x ^ y)) ^ y) + _sha1_blk(i) + 0x5A827999 + _sha1_rol(v, 5);    \
    w = _sha1_rol(w, 30);
#define _sha1_R2(v, w, x, y, z, i)                                             \
    z += (w ^ x ^ y) + _sha1_blk(i) + 0x6ED9EBA1 + _sha1_rol(v, 5);            \
    w = _sha1_rol(w, 30);
#define _sha1_R3(v, w, x, y, z, i)                                             \
    z += (((w | x) & y) | (w & x)) + _sha1_blk(i) + 0x8F1BBCDC +               \
         _sha1_rol(v, 5);                                                      \
    w = _sha1_rol(w, 30);
#define _sha1_R4(v, w, x, y, z, i)                                             \
    z += (w ^ x ^ y) + _sha1_blk(i) + 0xCA62C1D6 + _sha1_rol(v, 5);            \
    w = _sha1_rol(w, 30);

static void _rd_sha1_transform(u32 state[5], const u8 buffer[64]) {
    u32 l[16];

    for(int i = 0; i < 16; i++) {
        l[i] = rd_loadbe32(&buffer[(ptrdiff_t)i * 4]);
    }

    u32 a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];

    // clang-format off
    _sha1_R0(a, b, c, d, e, 0); _sha1_R0(e, a, b, c, d, 1); _sha1_R0(d, e, a, b, c, 2); _sha1_R0(c, d, e, a, b, 3); _sha1_R0(b, c, d, e, a, 4); _sha1_R0(a, b, c, d, e, 5); _sha1_R0(e, a, b, c, d, 6); _sha1_R0(d, e, a, b, c, 7); _sha1_R0(c, d, e, a, b, 8); _sha1_R0(b, c, d, e, a, 9); _sha1_R0(a, b, c, d, e, 10); _sha1_R0(e, a, b, c, d, 11); _sha1_R0(d, e, a, b, c, 12); _sha1_R0(c, d, e, a, b, 13); _sha1_R0(b, c, d, e, a, 14); _sha1_R0(a, b, c, d, e, 15);
    _sha1_R1(e, a, b, c, d, 16); _sha1_R1(d, e, a, b, c, 17); _sha1_R1(c, d, e, a, b, 18); _sha1_R1(b, c, d, e, a, 19);
    _sha1_R2(a, b, c, d, e, 20); _sha1_R2(e, a, b, c, d, 21); _sha1_R2(d, e, a, b, c, 22); _sha1_R2(c, d, e, a, b, 23); _sha1_R2(b, c, d, e, a, 24); _sha1_R2(a, b, c, d, e, 25); _sha1_R2(e, a, b, c, d, 26); _sha1_R2(d, e, a, b, c, 27); _sha1_R2(c, d, e, a, b, 28); _sha1_R2(b, c, d, e, a, 29); _sha1_R2(a, b, c, d, e, 30); _sha1_R2(e, a, b, c, d, 31); _sha1_R2(d, e, a, b, c, 32); _sha1_R2(c, d, e, a, b, 33); _sha1_R2(b, c, d, e, a, 34); _sha1_R2(a, b, c, d, e, 35); _sha1_R2(e, a, b, c, d, 36); _sha1_R2(d, e, a, b, c, 37); _sha1_R2(c, d, e, a, b, 38); _sha1_R2(b, c, d, e, a, 39);
    _sha1_R3(a, b, c, d, e, 40); _sha1_R3(e, a, b, c, d, 41); _sha1_R3(d, e, a, b, c, 42); _sha1_R3(c, d, e, a, b, 43); _sha1_R3(b, c, d, e, a, 44); _sha1_R3(a, b, c, d, e, 45); _sha1_R3(e, a, b, c, d, 46); _sha1_R3(d, e, a, b, c, 47); _sha1_R3(c, d, e, a, b, 48); _sha1_R3(b, c, d, e, a, 49); _sha1_R3(a, b, c, d, e, 50); _sha1_R3(e, a, b, c, d, 51); _sha1_R3(d, e, a, b, c, 52); _sha1_R3(c, d, e, a, b, 53); _sha1_R3(b, c, d, e, a, 54); _sha1_R3(a, b, c, d, e, 55); _sha1_R3(e, a, b, c, d, 56); _sha1_R3(d, e, a, b, c, 57); _sha1_R3(c, d, e, a, b, 58); _sha1_R3(b, c, d, e, a, 59);
    _sha1_R4(a, b, c, d, e, 60); _sha1_R4(e, a, b, c, d, 61); _sha1_R4(d, e, a, b, c, 62); _sha1_R4(c, d, e, a, b, 63); _sha1_R4(b, c, d, e, a, 64); _sha1_R4(a, b, c, d, e, 65); _sha1_R4(e, a, b, c, d, 66); _sha1_R4(d, e, a, b, c, 67); _sha1_R4(c, d, e, a, b, 68); _sha1_R4(b, c, d, e, a, 69); _sha1_R4(a, b, c, d, e, 70); _sha1_R4(e, a, b, c, d, 71); _sha1_R4(d, e, a, b, c, 72); _sha1_R4(c, d, e, a, b, 73); _sha1_R4(b, c, d, e, a, 74); _sha1_R4(a, b, c, d, e, 75); _sha1_R4(e, a, b, c, d, 76); _sha1_R4(d, e, a, b, c, 77); _sha1_R4(c, d, e, a, b, 78); _sha1_R4(b, c, d, e, a, 79);
    // clang-format on

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

void rd_i_hash_sha1_init(RDHash* self) {
    self->sha1.state[0] = 0x67452301;
    self->sha1.state[1] = 0xEFCDAB89;
    self->sha1.state[2] = 0x98BADCFE;
    self->sha1.state[3] = 0x10325476;
    self->sha1.state[4] = 0xC3D2E1F0;
    self->sha1.count = 0;
}

void rd_i_hash_sha1_update(RDHash* self, const void* data, usize len) {
    const u8* p = data;
    usize j = (usize)((self->sha1.count >> 3) & 63);
    usize i;

    self->sha1.count += (u64)len << 3;

    if(j + len > 63) {
        i = 64 - j;
        memcpy(&self->sha1.buffer[j], p, i);
        _rd_sha1_transform(self->sha1.state, self->sha1.buffer);

        for(; i + 63 < len; i += 64)
            _rd_sha1_transform(self->sha1.state, &p[i]);

        j = 0;
    }
    else
        i = 0;

    memcpy(&self->sha1.buffer[j], &p[i], len - i);
}

const u8* rd_i_hash_sha1_final(RDHash* self) {
    u8 finalcount[8];
    u64 bits = self->sha1.count;

    for(unsigned i = 0; i < 8; i++)
        finalcount[i] = (u8)(bits >> (56 - (i * 8)));

    u8 c = 0x80;
    rd_i_hash_sha1_update(self, &c, 1);

    while((self->sha1.count & 504) != 448) {
        c = 0x00;
        rd_i_hash_sha1_update(self, &c, 1);
    }

    rd_i_hash_sha1_update(self, finalcount, 8);

    for(unsigned i = 0; i < 5; i++)
        rd_storebe32(&self->sha1.digest[(size_t)i * 4], self->sha1.state[i]);

    return self->sha1.digest;
}
