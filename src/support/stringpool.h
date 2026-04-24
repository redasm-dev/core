#pragma once

#include <redasm/config.h>

typedef struct RDStringPool {
    char** data;
    usize length;
    usize capacity;
} RDStringPool;

const char* rd_i_strpool_intern(RDStringPool* self, const char* s);
void rd_i_strpool_destroy(RDStringPool* self);
