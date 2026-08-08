#pragma once

#include <redasm/hooks.h>

typedef struct RDHookKey {
    const char* name;
    RDHookKind kind;
} RDHookKey;

typedef struct RDHookItem {
    const char* name;
    RDHookKind kind;
    RDHookFunc fn;
    void* userdata;
} RDHookItem;

typedef struct RDHookItemVect {
    RDHookItem* data;
    usize length;
    usize capacity;
} RDHookItemVect;
