#include "schema.h"
#include "core/state.h"
#include "support/containers.h"
#include "support/logging.h"
#include <string.h>

static const char* const RD_KB_KIND_VALUES[] = {
    "struct",
    "enum",
    "function",
    NULL,
};

static const RDKBFieldSchema RD_KB_SCHEMA_TYPE[] = {
    {
        .key = "kind",
        .kind = RD_KB_STR,
        .required = true,
        .str_values = RD_KB_KIND_VALUES,
    },

    {.key = "members", .kind = RD_KB_ARRAY, .required = true},
};

static const RDKBFieldSchema RD_KB_SCHEMA_MEMBER[] = {
    {.key = "type", .kind = RD_KB_STR, .required = true},
    {.key = "name", .kind = RD_KB_STR, .required = true},
    {.key = "count", .kind = RD_KB_INT, .required = false},
    {.key = "mod", .kind = RD_KB_STR, .required = false},
};

static const char* _rd_kb_kind_str(RDKBObjectKind kind) {
    switch(kind) {
        case RD_KB_STR: return "STR";
        case RD_KB_INT: return "INT";
        case RD_KB_FLOAT: return "FLOAT";
        case RD_KB_BOOL: return "BOOL";
        case RD_KB_DATE: return "DATE";
        case RD_KB_TIME: return "TIME";
        case RD_KB_DATETIME: return "DATETIME";
        case RD_KB_ARRAY: return "ARRAY";
        case RD_KB_TABLE: return "TABLE";

        default: break;
    }

    return "UNKNOWN";
}

static const char* _rd_kb_get_str_values(const RDKBFieldSchema* s) {
    RDCharVect* schema_buf = &rd_i_state.kb.schema_buf;
    const char* const* str_v = s->str_values;
    str_clear(schema_buf);

    while(*str_v) {
        if(str_v > s->str_values) str_append(schema_buf, ", ");
        str_append(schema_buf, *str_v);
        str_v++;
    }

    return rd_i_state.kb.schema_buf.data;
}

static bool _rd_kb_validate_kind(const RDKBObject* o,
                                 const RDKBFieldSchema* s) {
    assert(o);
    RDKBObjectKind t = rd_kbobject_get_kind(o);
    if(t == s->kind) return true;

    const char* got_kind = _rd_kb_kind_str(t);
    const char* expected_kind = _rd_kb_kind_str(s->kind);

    LOG_FAIL("expected kind '%s', got '%s' for key '%s'", expected_kind,
             got_kind, s->key);

    return true;
}

static bool _rd_kb_validate_str_values(const char* v,
                                       const RDKBFieldSchema* s) {
    assert(v);
    assert(s->str_values);

    const char* const* str_v = s->str_values;
    bool found = false;

    while(*str_v) {
        if(!strcmp(v, *str_v)) {
            found = true;
            break;
        }

        str_v++;
    }

    if(!found) {
        LOG_FAIL("value '%s' is not valid, possible values are %s", v,
                 _rd_kb_get_str_values(s));
    }

    return found;
}

static bool _rd_kb_validate_schema(const RDKBObject* obj,
                                   const RDKBFieldSchema* schema, usize n) {
    RDKBObjectKind k = rd_kbobject_get_kind(obj);

    if(k != RD_KB_TABLE) {
        LOG_FAIL("cannot validate object of type '%s', only table allowed",
                 _rd_kb_kind_str(k));
        return false;
    }

    for(usize i = 0; i < n; i++) {
        const RDKBFieldSchema* s = &schema[i];
        const RDKBObject* o = rd_kbobject_get(obj, s->key);

        if(!o) {
            if(s->required) {
                LOG_FAIL("missing required field '%s'", s->key);
                return false;
            }

            return true;
        }

        if(!_rd_kb_validate_kind(o, s)) return false;

        if(rd_kbobject_get_kind(o) == RD_KB_STR && s->str_values) {
            const char* v = rd_kbobject_to_str(o);
            if(!_rd_kb_validate_str_values(v, s)) return false;
        }
    }

    return true;
}

bool rd_i_kb_validate_type(const RDKBObject* obj) {
    return _rd_kb_validate_schema(obj, RD_KB_SCHEMA_TYPE,
                                  rd_count_of(RD_KB_SCHEMA_TYPE));
}

bool rd_i_kb_validate_member(const RDKBObject* obj) {
    return _rd_kb_validate_schema(obj, RD_KB_SCHEMA_MEMBER,
                                  rd_count_of(RD_KB_SCHEMA_MEMBER));
}
