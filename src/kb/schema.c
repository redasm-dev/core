#include "schema.h"
#include "kb/object.h"
#include "support/tomlschema.h"

static const char* rd_kb_mod_values[] = {
    "ptr",
    "cptr",
    NULL,
};

static const char* rd_kb_mod_external[] = {
    "imported",
    "exported",
    NULL,
};

static const char* rd_kb_arg_order_values[] = {
    "rtl",
    "ltr",
    NULL,
};

static const char* rd_kb_stack_cleanup_values[] = {
    "caller",
    "callee",
    NULL,
};

// clang-format off
static const RDTomlSchema RD_KB_SCHEMA_MANIFEST[] = {
    {.key = "dependencies", .type = TOML_ARRAY, .optional = true, .array_type = &RD_TOMLSCHEMA_ITEM_STRING},
    {.key = "callconv", .type = TOML_STRING, .optional = true},
    {0},
};

static const RDTomlSchema RD_KB_SCHEMA_TYPE[] = {
    {.key = "name", .type = TOML_STRING},
    {.key = "count", .type = TOML_INT64, .optional = true},
    {.key = "mod", .type = TOML_STRING, .optional = true, .string_values = rd_kb_mod_values},
    {0},
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
    {.key = "mod", .type = TOML_STRING, .optional = true, .string_values = rd_kb_mod_values},
    {0},
};

static const RDTomlSchema RD_KB_SCHEMA_COMPOUND[] = {
    {.key = "members", .type = TOML_ARRAY, .array_type = RD_KB_SCHEMA_PARAM},
    {0},
};

static const RDTomlSchema RD_KB_SCHEMA_ENUM[] = {
    {.key = "base_type", .type = TOML_STRING},
    {.key = "members", .type = TOML_ARRAY, .array_type = RD_KB_SCHEMA_ENUM_PARAM},
    {0},
};

static const RDTomlSchema RD_KB_SCHEMA_RET[] = {
    {.key = "type", .type = TOML_STRING},
    {.key = "count", .type = TOML_INT64, .optional = true},
    {.key = "mod", .type = TOML_STRING, .optional = true, .string_values = rd_kb_mod_values},
    {0},
};

static const RDTomlSchema RD_KB_SCHEMA_FUNCTION[] = {
    {.key = "callconv", .type = TOML_STRING, .optional = true},
    {.key = "noret", .type = TOML_BOOLEAN, .optional = true},
    {.key = "ret", .type = TOML_TABLE, .table_type = RD_KB_SCHEMA_RET},
    {.key = "args", .type = TOML_ARRAY, .array_type = RD_KB_SCHEMA_PARAM},
    {0},
};

static const RDTomlSchema RD_KB_SCHEMA_SYMBOL[] = {
    {.key = "address", .type = TOML_INT64},
    {.key = "module", .type = TOML_STRING, .optional = true},
    {.key = "external", .type = TOML_STRING, .string_values = rd_kb_mod_external, .optional = true},
    {.key = "function", .type = TOML_BOOLEAN, .optional = true},
    {.key = "type", .type = TOML_TABLE, .table_type = RD_KB_SCHEMA_TYPE, .optional = true},
    {.key = "comment", .type = TOML_STRING, .optional = true},
    {0},
};

static const RDTomlSchema RD_KB_SCHEMA_CALLCONV[] = {
    {.key = "arg_regs", .type = TOML_ARRAY, .optional = true, .array_type = &RD_TOMLSCHEMA_ITEM_STRING},
    {.key = "arg_order", .type = TOML_STRING, .string_values = rd_kb_arg_order_values},
    {.key = "shadow_space", .type = TOML_INT64, .optional = true},
    {.key = "stack_cleanup", .type = TOML_STRING, .string_values = rd_kb_stack_cleanup_values},
    {0},
};
// clang-format off

bool rd_i_kb_validate_manifest(const RDKBObject* obj) {
    const toml_datum_t* d = rd_i_kb_to_datum(obj);
    assert(d);
    return rd_i_toml_validate_schema(*d, RD_KB_SCHEMA_MANIFEST);
}

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

bool rd_i_kb_validate_symbol(const RDKBObject* obj) {
    const toml_datum_t* d = rd_i_kb_to_datum(obj);
    assert(d);
    return rd_i_toml_validate_schema(*d, RD_KB_SCHEMA_SYMBOL);
}

bool rd_i_kb_validate_callconv(const RDKBObject* obj) {
    const toml_datum_t* d = rd_i_kb_to_datum(obj);
    assert(d);
    return rd_i_toml_validate_schema(*d, RD_KB_SCHEMA_CALLCONV);
}
