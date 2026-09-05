#include "hash.h"
#include "support/hash/adler32.h"
#include "support/hash/sha1.h"
#include <redasm/allocator.h>
#include <redasm/common.h>
#include <redasm/support/logging.h>
#include <string.h>

static const RDHashOps HASH_OPS[] = {
    [RD_HASH_SHA1] =
        {
            .digest_length = RD_HASH_SHA1_LENGTH,
            .init = rd_i_hash_sha1_init,
            .update = rd_i_hash_sha1_update,
            .final = rd_i_hash_sha1_final,
        },
    [RD_HASH_ADLER32] =
        {
            .digest_length = RD_HASH_ADLER32_LENGTH,
            .init = rd_i_hash_adler32_init,
            .update = rd_i_hash_adler32_update,
            .final = rd_i_hash_adler32_final,
        },
};

RDHash* rd_hash_create(RDHashKind kind) {
    if(kind < 0 || (usize)kind >= rd_count_of(HASH_OPS) ||
       !HASH_OPS[kind].init) {
        RD_LOG_FAIL("invalid hash kind %d", kind);
        return NULL;
    }

    RDHash* self = rd_alloc0(1, sizeof(*self));
    self->kind = kind;
    self->ops = &HASH_OPS[kind];
    self->ops->init(self);
    return self;
}

void rd_hash_destroy(RDHash* self) {
    if(self) rd_free(self);
}

void rd_hash_update(RDHash* self, const char* data, usize n) {
    if(!self || !n) return;

    if(self->finalized) {
        RD_LOG_FAIL("update after final");
        return;
    }

    self->ops->update(self, data, n);
}

void rd_hash_reset(RDHash* self) {
    if(!self) return;
    self->finalized = false;
    self->ops->init(self);
}

const u8* rd_hash_final(RDHash* self) {
    if(!self) return NULL;

    if(!self->finalized) {
        self->finalized = true;
        return self->ops->final(self);
    }

    switch(self->kind) {
        case RD_HASH_SHA1: return self->sha1.digest;
        case RD_HASH_ADLER32: return self->adler32.digest;
        default: break;
    }

    return NULL;
}

bool rd_hash_final_to(RDHash* self, void* dst, usize cap) {
    if(!self || !dst) return false;
    if(cap < self->ops->digest_length) return false;

    const u8* d = rd_hash_final(self);
    if(!d) return false;

    memcpy(dst, d, self->ops->digest_length);
    return true;
}

usize rd_hash_get_length(const RDHash* self) {
    return self ? self->ops->digest_length : 0;
}
