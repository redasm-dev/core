#pragma once

#include "support/hash/hash.h"

void rd_i_hash_sha1_init(RDHash* self);
void rd_i_hash_sha1_update(RDHash* self, const void* data, usize len);
const u8* rd_i_hash_sha1_final(RDHash* self);
