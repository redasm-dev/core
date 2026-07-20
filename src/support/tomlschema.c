#include "tomlschema.h"
#include "support/containers.h"
#include "support/utils.h"
#include <redasm/support/logging.h>
#include <string.h>

static bool _rd_toml_expect(const toml_datum_t* d, const RDTomlSchema* s) {
    assert(s->key);
    assert(s->type != TOML_UNKNOWN);

    if(d->type == TOML_UNKNOWN) {
        if(s->optional) return true;
        RD_LOG_FAIL("missing property '%s'", s->key);
        return false;
    }

    if(d->type != s->type) {
        RD_LOG_FAIL("expected %s for '%s' property, found %s ",
                    rd_i_toml_type_str(s->type), s->key,
                    rd_i_toml_type_str(d->type));
        return false;
    }

    return true;
}

static bool _rd_toml_expect_string_values(const toml_datum_t* d,
                                          const RDTomlSchema* s) {
    assert(d->type == TOML_STRING);

    if(!s->string_values) return true;

    const char* const* str_v = s->string_values;
    bool found = false;

    while(*str_v) {
        if(!strcmp(d->u.s, *str_v)) {
            found = true;
            break;
        }

        str_v++;
    }

    if(!found) {
        RDCharVect values = {0};
        str_v = s->string_values;

        while(*str_v) {
            if(str_v > s->string_values) str_append(&values, ", ");
            str_append(&values, *str_v);
            str_v++;
        }

        RD_LOG_FAIL("value '%s' is not valid, possible values are %s", d->u.s,
                    values.data);

        vect_destroy(&values);
    }

    return found;
}

static bool _rd_toml_expect_array_of_elem_type(const toml_datum_t* d,
                                               const RDTomlSchema* s) {
    const RDTomlSchema* elem = s->array_type;

    for(int i = 0; i < d->u.arr.size; i++) {
        toml_datum_t e = d->u.arr.elem[i];

        if(e.type != elem->type) {
            RD_LOG_FAIL(
                "expected array of %s for '%s' property, found %s at index "
                "%d",
                rd_i_toml_type_str(elem->type), s->key,
                rd_i_toml_type_str(e.type), i);
            return false;
        }

        if(e.type == TOML_STRING && !_rd_toml_expect_string_values(&e, elem))
            return false;
    }

    return true;
}

static bool _rd_toml_expect_array_of_tables(const toml_datum_t* d,
                                            const RDTomlSchema* s) {
    for(int i = 0; i < d->u.arr.size; i++) {
        if(!rd_i_toml_validate_schema(d->u.arr.elem[i], s->array_type))
            return false;
    }

    return true;
}

static bool _rd_toml_expect_array_type(const toml_datum_t* d,
                                       const RDTomlSchema* s) {
    assert(d->type == TOML_ARRAY);

    if(!s->array_type) return true;
    if(!s->array_type->key) return _rd_toml_expect_array_of_elem_type(d, s);
    return _rd_toml_expect_array_of_tables(d, s);
}

static bool _rd_toml_expect_table_type(const toml_datum_t* d,
                                       const RDTomlSchema* s) {
    assert(d->type == TOML_TABLE);

    if(!s->table_type) return true;
    return rd_i_toml_validate_schema(*d, s->table_type);
}

const char* rd_i_toml_type_str(toml_type_t t) {
    switch(t) {
        case TOML_STRING: return "STR";
        case TOML_INT64: return "INT";
        case TOML_FP64: return "FLOAT";
        case TOML_BOOLEAN: return "BOOL";
        case TOML_DATE: return "DATE";
        case TOML_TIME: return "TIME";
        case TOML_DATETIME: return "DATETIME";
        case TOML_ARRAY: return "ARRAY";
        case TOML_TABLE: return "TABLE";

        default: break;
    }

    return "UNKNOWN";
}

bool rd_i_toml_validate_schema(toml_datum_t root, const RDTomlSchema* schema) {
    if(!schema) {
        RD_LOG_FAIL("invalid schema definition");
        return false;
    }

    if(root.type != TOML_TABLE) {
        RD_LOG_FAIL("root is not %s, got %s", rd_i_toml_type_str(TOML_TABLE),
                    rd_i_toml_type_str(root.type));
        return false;
    }

    const RDTomlSchema* s = schema;

    while(s->key) {
        toml_datum_t d = toml_seek(root, s->key);
        if(!_rd_toml_expect(&d, s)) return false;

        if(d.type == TOML_STRING && !_rd_toml_expect_string_values(&d, s))
            return false;

        if(d.type == TOML_TABLE && !_rd_toml_expect_table_type(&d, s))
            return false;

        if(d.type == TOML_ARRAY && !_rd_toml_expect_array_type(&d, s))
            return false;

        s++;
    }

    return true;
}
