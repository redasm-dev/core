#include "support/containers.h"
#include "support/utils.h"
#include <redasm/support/datum.h>
#include <redasm/support/logging.h>
#include <string.h>

static bool _rd_datum_expect(const RDDatum* d, const RDDatumSchema* s) {
    RDDatumKind kind = rd_datum_get_kind(d); // NULL is RD_DATUM_UNKNOWN

    if(kind == RD_DATUM_UNKNOWN) {
        if(s->optional) return true;
        RD_LOG_FAIL("missing property '%s'", s->key);
        return false;
    }

    if(kind != s->kind) {
        RD_LOG_FAIL("expected %s for '%s' property, found %s",
                    rd_datum_kind_str(s->kind), s->key,
                    rd_datum_kind_str(kind));
        return false;
    }

    return true;
}

static bool _rd_datum_expect_string_values(const RDDatum* d,
                                           const RDDatumSchema* s) {
    if(!s->string_values) return true;

    const char* str = rd_datum_to_str(d);

    for(const char* const* v = s->string_values; *v; v++) {
        if(!strcmp(str, *v)) return true;
    }

    RDCharVect values = {0};

    for(const char* const* v = s->string_values; *v; v++) {
        if(v > s->string_values) str_append(&values, ", ");
        str_append(&values, *v);
    }

    RD_LOG_FAIL("value '%s' is not valid, possible values are %s", str,
                values.data);

    vect_destroy(&values);
    return false;
}

static bool _rd_datum_expect_array_of_elem_kind(const RDDatum* d,
                                                const RDDatumSchema* s) {
    const RDDatumSchema* elem = s->array_type;

    for(usize i = 0; i < rd_datum_get_length(d); i++) {
        const RDDatum* e = rd_datum_array_at(d, i);
        RDDatumKind kind = rd_datum_get_kind(e);

        if(kind != elem->kind) {
            RD_LOG_FAIL("expected array of %s for '%s' property, found %s at "
                        "index %zu",
                        rd_datum_kind_str(elem->kind), s->key,
                        rd_datum_kind_str(kind), i);
            return false;
        }

        if(kind == RD_DATUM_STRING && !_rd_datum_expect_string_values(e, elem))
            return false;
    }

    return true;
}

static bool _rd_datum_expect_array_of_tables(const RDDatum* d,
                                             const RDDatumSchema* s) {
    for(usize i = 0; i < rd_datum_get_length(d); i++) {
        if(!rd_datum_validate(rd_datum_array_at(d, i), s->array_type))
            return false;
    }

    return true;
}

static bool _rd_datum_expect_array_type(const RDDatum* d,
                                        const RDDatumSchema* s) {
    if(!s->array_type) return true;

    // an element schema without a key describes scalars, otherwise tables
    if(!s->array_type->key) return _rd_datum_expect_array_of_elem_kind(d, s);
    return _rd_datum_expect_array_of_tables(d, s);
}

const char* rd_datum_kind_str(RDDatumKind kind) {
    switch(kind) {
        case RD_DATUM_STRING: return "STRING";
        case RD_DATUM_INT: return "INT";
        case RD_DATUM_FLOAT: return "FLOAT";
        case RD_DATUM_BOOL: return "BOOL";
        case RD_DATUM_DATE: return "DATE";
        case RD_DATUM_TIME: return "TIME";
        case RD_DATUM_DATETIME: return "DATETIME";
        case RD_DATUM_ARRAY: return "ARRAY";
        case RD_DATUM_TABLE: return "TABLE";
        default: break;
    }

    return "UNKNOWN";
}

bool rd_datum_validate(const RDDatum* root, const RDDatumSchema* schema) {
    if(!schema) {
        RD_LOG_FAIL("invalid schema definition");
        return false;
    }

    RDDatumKind rootkind = rd_datum_get_kind(root);

    if(rootkind != RD_DATUM_TABLE) {
        RD_LOG_FAIL("root is not %s, got %s",
                    rd_datum_kind_str(RD_DATUM_STRING),
                    rd_datum_kind_str(rootkind));
        return false;
    }

    for(const RDDatumSchema* s = schema; s->key; s++) {
        // dotted keys like "format.api_version" walk nested tables
        const RDDatum* d = rd_datum_get(root, s->key);
        if(!_rd_datum_expect(d, s)) return false;

        switch(rd_datum_get_kind(d)) {
            case RD_DATUM_STRING:
                if(!_rd_datum_expect_string_values(d, s)) return false;
                break;

            case RD_DATUM_TABLE:
                if(s->table_type && !rd_datum_validate(d, s->table_type))
                    return false;
                break;

            case RD_DATUM_ARRAY:
                if(!_rd_datum_expect_array_type(d, s)) return false;
                break;

            default: break;
        }
    }

    return true;
}
