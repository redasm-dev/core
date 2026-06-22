#pragma once

#include <tomlc17.h>

typedef struct RDTomlSchema {
    const char* key;
    toml_type_t type;
    bool optional;

    union {
        const char** string_values;            // TOML_STRING
        const struct RDTomlSchema* array_type; // TOML_ARRAY
        const struct RDTomlSchema* table_type; // TOML_TABLE
    };
} RDTomlSchema;

const char* rd_i_toml_type_str(toml_type_t t);
bool rd_i_toml_validate_schema(toml_datum_t root, const RDTomlSchema* schema);
