#pragma once

#include <redasm/kb.h>

typedef struct RDKBFieldSchema {
    const char* key;
    RDKBObjectKind kind;
    const char* mod;
    bool required;
    const char* const* str_values;
} RDKBFieldSchema;

bool rd_i_kb_validate_function(const RDKBObject* obj);
bool rd_i_kb_validate_compound(const RDKBObject* obj);
bool rd_i_kb_validate_enum(const RDKBObject* obj);
