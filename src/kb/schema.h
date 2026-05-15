#pragma once

#include <redasm/kb.h>

typedef struct RDKBFieldSchema {
    const char* key;
    RDKBObjectKind kind;
    bool required;
    const char* const* str_values;
} RDKBFieldSchema;

bool rd_i_kb_validate_type(const RDKBObject* obj);
bool rd_i_kb_validate_member(const RDKBObject* obj);
