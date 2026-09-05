#pragma once

#include "support/hash/hash.h"

void rd_i_hash_adler32_init(RDHash* self);
void rd_i_hash_adler32_update(RDHash* self, const void* data, usize n);
const u8* rd_i_hash_adler32_final(RDHash* self);
