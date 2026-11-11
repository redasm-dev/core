#include "schema.h"

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
static const RDDatumSchema RD_KB_SCHEMA_MANIFEST[] = {
    {.key = "dependencies", .kind = RD_DATUM_ARRAY, .optional = true, .array_type = &RD_DATUM_ITEM_STRING},
    {.key = "callconv", .kind = RD_DATUM_STRING, .optional = true},
    {0},
};

static const RDDatumSchema RD_KB_SCHEMA_TYPE[] = {
    {.key = "name", .kind = RD_DATUM_STRING},
    {.key = "count", .kind = RD_DATUM_INT, .optional = true},
    {.key = "mod", .kind = RD_DATUM_STRING, .optional = true, .string_values = rd_kb_mod_values},
    {0},
};

static const RDDatumSchema RD_KB_SCHEMA_ENUM_PARAM[] = {
    {.key = "name", .kind = RD_DATUM_STRING},
    {.key = "value", .kind = RD_DATUM_INT},
    {0},
};

static const RDDatumSchema RD_KB_SCHEMA_PARAM[] = {
    {.key = "type", .kind = RD_DATUM_STRING},
    {.key = "name", .kind = RD_DATUM_STRING},
    {.key = "count", .kind = RD_DATUM_INT, .optional = true},
    {.key = "mod", .kind = RD_DATUM_STRING, .optional = true, .string_values = rd_kb_mod_values},
    {0},
};

static const RDDatumSchema RD_KB_SCHEMA_COMPOUND[] = {
    {.key = "members", .kind = RD_DATUM_ARRAY, .array_type = RD_KB_SCHEMA_PARAM},
    {0},
};

static const RDDatumSchema RD_KB_SCHEMA_ENUM[] = {
    {.key = "base_type", .kind = RD_DATUM_STRING},
    {.key = "members", .kind = RD_DATUM_ARRAY, .array_type = RD_KB_SCHEMA_ENUM_PARAM},
    {0},
};

static const RDDatumSchema RD_KB_SCHEMA_RET[] = {
    {.key = "type", .kind = RD_DATUM_STRING},
    {.key = "count", .kind = RD_DATUM_INT, .optional = true},
    {.key = "mod", .kind = RD_DATUM_STRING, .optional = true, .string_values = rd_kb_mod_values},
    {0},
};

static const RDDatumSchema RD_KB_SCHEMA_FUNCTION[] = {
    {.key = "callconv", .kind = RD_DATUM_STRING, .optional = true},
    {.key = "noret", .kind = RD_DATUM_BOOL, .optional = true},
    {.key = "ret", .kind = RD_DATUM_TABLE, .table_type = RD_KB_SCHEMA_RET},
    {.key = "args", .kind = RD_DATUM_ARRAY, .array_type = RD_KB_SCHEMA_PARAM},
    {0},
};

static const RDDatumSchema RD_KB_SCHEMA_SYMBOL[] = {
    {.key = "address", .kind = RD_DATUM_INT},
    {.key = "module", .kind = RD_DATUM_STRING, .optional = true},
    {.key = "external", .kind = RD_DATUM_STRING, .string_values = rd_kb_mod_external, .optional = true},
    {.key = "function", .kind = RD_DATUM_BOOL, .optional = true},
    {.key = "type", .kind = RD_DATUM_TABLE, .table_type = RD_KB_SCHEMA_TYPE, .optional = true},
    {.key = "comment", .kind = RD_DATUM_STRING, .optional = true},
    {0},
};

static const RDDatumSchema RD_KB_SCHEMA_CALLCONV[] = {
    {.key = "arg_regs", .kind = RD_DATUM_ARRAY, .optional = true, .array_type = &RD_DATUM_ITEM_STRING},
    {.key = "arg_order", .kind = RD_DATUM_STRING, .string_values = rd_kb_arg_order_values},
    {.key = "shadow_space", .kind = RD_DATUM_INT, .optional = true},
    {.key = "stack_cleanup", .kind = RD_DATUM_STRING, .string_values = rd_kb_stack_cleanup_values},
    {0},
};
// clang-format off

bool rd_i_kb_validate_manifest(const RDDatum* obj) {
    return rd_datum_validate(obj, RD_KB_SCHEMA_MANIFEST);
}

bool rd_i_kb_validate_function(const RDDatum* obj) {
    return rd_datum_validate(obj, RD_KB_SCHEMA_FUNCTION);
}

bool rd_i_kb_validate_compound(const RDDatum* obj) {
    return rd_datum_validate(obj, RD_KB_SCHEMA_COMPOUND);
}

bool rd_i_kb_validate_enum(const RDDatum* obj) {
    return rd_datum_validate(obj, RD_KB_SCHEMA_ENUM);
}

bool rd_i_kb_validate_symbol(const RDDatum* obj) {
    return rd_datum_validate(obj, RD_KB_SCHEMA_SYMBOL);
}

bool rd_i_kb_validate_callconv(const RDDatum* obj) {
    return rd_datum_validate(obj, RD_KB_SCHEMA_CALLCONV);
}
