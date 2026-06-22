#include "schema.h"
#include "kb/object.h"
#include "support/tomlschema.h"

static const char* rd_kb_kind_compound_values[] = {
    "struct",
    "union",
    NULL,
};

static const char* rd_kb_mod_values[] = {
    "ptr",
    "cptr",
    NULL,
};

static const RDTomlSchema RD_KB_SCHEMA_ENUM_PARAM[] = {
    {.key = "name", .type = TOML_STRING},
    {.key = "value", .type = TOML_INT64},
    {0},
};

static const RDTomlSchema RD_KB_SCHEMA_PARAM[] = {
    {.key = "type", .type = TOML_STRING},
    {.key = "name", .type = TOML_STRING},
    {.key = "count", .type = TOML_INT64, .optional = true},

    {
        .key = "mod",
        .type = TOML_STRING,
        .optional = true,
        .string_values = rd_kb_mod_values,
    },
    {0},
};

static const RDTomlSchema RD_KB_SCHEMA_COMPOUND[] = {
    {
        .key = "kind",
        .type = TOML_STRING,
        .string_values = rd_kb_kind_compound_values,
    },
    {.key = "members", .type = TOML_ARRAY, .array_type = RD_KB_SCHEMA_PARAM},
    {0},
};

static const RDTomlSchema RD_KB_SCHEMA_ENUM[] = {
    {
        .key = "base_type",
        .type = TOML_STRING,
    },
    {
        .key = "members",
        .type = TOML_ARRAY,
        .array_type = RD_KB_SCHEMA_ENUM_PARAM,
    },
    {0},
};

static const RDTomlSchema RD_KB_SCHEMA_RET[] = {
    {.key = "type", .type = TOML_STRING},
    {.key = "count", .type = TOML_INT64, .optional = true},

    {
        .key = "mod",
        .type = TOML_STRING,
        .optional = true,
        .string_values = rd_kb_mod_values,
    },
    {0},
};

static const RDTomlSchema RD_KB_SCHEMA_FUNCTION[] = {
    {.key = "cc", .type = TOML_STRING, .optional = true},
    {.key = "noret", .type = TOML_BOOLEAN, .optional = true},
    {
        .key = "ret",
        .type = TOML_TABLE,
        .table_type = RD_KB_SCHEMA_RET,
    },
    {.key = "args", .type = TOML_ARRAY, .array_type = RD_KB_SCHEMA_PARAM},
    {0},
};

bool rd_i_kb_validate_function(const RDKBObject* obj) {
    const toml_datum_t* d = rd_i_kb_to_datum(obj);
    assert(d);
    return rd_i_toml_validate_schema(*d, RD_KB_SCHEMA_FUNCTION);
}

bool rd_i_kb_validate_compound(const RDKBObject* obj) {
    const toml_datum_t* d = rd_i_kb_to_datum(obj);
    assert(d);
    return rd_i_toml_validate_schema(*d, RD_KB_SCHEMA_COMPOUND);
}

bool rd_i_kb_validate_enum(const RDKBObject* obj) {
    const toml_datum_t* d = rd_i_kb_to_datum(obj);
    assert(d);
    return rd_i_toml_validate_schema(*d, RD_KB_SCHEMA_ENUM);
}
