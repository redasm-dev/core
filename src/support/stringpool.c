#include "stringpool.h"
#include "support/containers.h"
#include "support/hash.h"
#include <redasm/allocator.h>
#include <redasm/support/utils.h>
#include <stdio.h>
#include <string.h>

#define RD_STRINGPOOL_INITIAL_CAPACITY 512

static size_t _rd_strpool_hash(const void* e) {
    const RDStringPoolEntry* entry = (const RDStringPoolEntry*)e;
    return rd_i_murmur3(entry->value, strlen(entry->value));
}

static bool _rd_strpool_equal(const void* a, const void* b) {
    const RDStringPoolEntry* ea = (const RDStringPoolEntry*)a;
    const RDStringPoolEntry* eb = (const RDStringPoolEntry*)b;
    if(ea->value == eb->value) return true;
    return !strcmp(ea->value, eb->value);
}

void rd_i_strpool_init(RDStringPool* self) {
    *self = (RDStringPool){
        .hash = _rd_strpool_hash,
        .equal = _rd_strpool_equal,
    };

    hmap_reserve(self, RD_STRINGPOOL_INITIAL_CAPACITY);
}

const char* rd_i_strpool_intern(RDStringPool* self, const char* s) {
    if(!s) return NULL;

    RDStringPoolEntry key = {.value = s};

    const RDStringPoolEntry* found = hmap_get(self, &key);
    if(found) return found->value;

    key.value = rd_strdup(s);
    hmap_set(self, &key);
    return key.value;
}

void rd_i_strpool_destroy(RDStringPool* self) {
    RDStringPoolEntry* it;
    hmap_each(it, self) { rd_free((char*)it->value); }
    hmap_destroy(self);
}
