#pragma once

#include "support/containers.h"
#include <redasm/config.h>

typedef struct RDStringPoolEntry {
    HMapHeader hmap_hdr;
    const char* value; // interned string pointer
} RDStringPoolEntry;

typedef struct RDStringPool {
    RDStringPoolEntry* data;
    usize length;
    usize capacity;
    usize tombstones;
    HMapHash hash;
    HMapEqual equal;
} RDStringPool;

void rd_i_strpool_init(RDStringPool* self);
void rd_i_strpool_deinit(RDStringPool* self);
const char* rd_i_strpool_intern(RDStringPool* self, const char* s);
