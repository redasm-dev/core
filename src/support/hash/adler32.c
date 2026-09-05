#include "adler32.h"

#define RD_HASH_ADLER32_MOD 65521
#define RD_HASH_ADLER32_NMAX 5552

void rd_i_hash_adler32_init(RDHash* self) {
    self->adler32.a = 1;
    self->adler32.b = 0;
}

void rd_i_hash_adler32_update(RDHash* self, const void* data, usize n) {
    const u8* p = data;
    u32 a = self->adler32.a, b = self->adler32.b;

    while(n) {
        usize k = n < RD_HASH_ADLER32_NMAX ? n : RD_HASH_ADLER32_NMAX;
        n -= k;

        while(k--) {
            a += *p++;
            b += a;
        }

        a %= RD_HASH_ADLER32_MOD;
        b %= RD_HASH_ADLER32_MOD;
    }

    self->adler32.a = a;
    self->adler32.b = b;
}

const u8* rd_i_hash_adler32_final(RDHash* self) {
    u32 v = (self->adler32.b << 16) | self->adler32.a;
    self->adler32.digest[0] = (u8)(v >> 24);
    self->adler32.digest[1] = (u8)(v >> 16);
    self->adler32.digest[2] = (u8)(v >> 8);
    self->adler32.digest[3] = (u8)(v >> 0);
    return self->adler32.digest;
}
