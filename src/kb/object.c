#include "object.h"
#include "core/state.h"
#include "support/containers.h"
#include <redasm/kb.h>
#include <string.h>

// we need pointers, do now use toml_get
static const toml_datum_t* _rd_kbobject_get(const toml_datum_t* datum,
                                            const char* key) {
    if(!datum || datum->type != TOML_TABLE) return NULL;

    int n = datum->u.tab.size;
    const char** pkey = datum->u.tab.key;
    toml_datum_t* pvalue = datum->u.tab.value;

    for(int i = 0; i < n; i++) {
        if(strcmp(pkey[i], key) != 0) continue;
        return &pvalue[i];
    }

    return NULL;
}

// we need pointers, do now use toml_seek
static const RDKBObject* _rd_kbobject_seek(const RDKBObject* self,
                                           const char* key) {
    const toml_datum_t* datum = rd_i_kb_to_datum(self);
    if(!self || datum->type != TOML_TABLE || !key) return NULL;

    str_clear(&rd_i_state.kb.key_buf);
    str_append(&rd_i_state.kb.key_buf, key);

    char* p = rd_i_state.kb.key_buf.data;
    const toml_datum_t* cur = datum;

    while(cur && datum->type == TOML_TABLE) {
        char* q = strchr(p, '.');

        if(q) {
            *q = 0;
            cur = _rd_kbobject_get(cur, p);
            p = q + 1;
            continue;
        }

        return rd_i_kb_from_datum(_rd_kbobject_get(cur, p));
    }

    return NULL;
}

usize rd_kbobject_get_length(const RDKBObject* self) {
    const toml_datum_t* datum = rd_i_kb_to_datum(self);
    if(datum->type == TOML_ARRAY) return (usize)datum->u.arr.size;
    if(datum->type == TOML_TABLE) return (usize)datum->u.tab.size;
    return 0;
}

RDKBObjectKind rd_kbobject_get_kind(const RDKBObject* self) {
    const toml_datum_t* datum = rd_i_kb_to_datum(self);

    if(datum) {
        switch(datum->type) {
            case TOML_UNKNOWN: return RD_KB_UNKNOWN;
            case TOML_STRING: return RD_KB_STR;
            case TOML_INT64: return RD_KB_INT;
            case TOML_FP64: return RD_KB_FLOAT;
            case TOML_BOOLEAN: return RD_KB_BOOL;
            case TOML_DATE: return RD_KB_DATE;
            case TOML_ARRAY: return RD_KB_ARRAY;
            case TOML_TABLE: return RD_KB_TABLE;
            case TOML_TIME: return RD_KB_TIME;

            case TOML_DATETIME:
            case TOML_DATETIMETZ: return RD_KB_DATETIME;

            default: break;
        }
    }

    return RD_KB_UNKNOWN;
}

const RDKBObject* rd_kbobject_get(const RDKBObject* self, const char* key) {
    return self ? _rd_kbobject_seek(self, key) : NULL;
}

const char* rd_kbobject_get_str(const RDKBObject* self, const char* key) {
    if(!self) return NULL;

    const RDKBObject* v = _rd_kbobject_seek(self, key);
    return rd_kbobject_to_str(v);
}

bool rd_kbobject_get_bool(const RDKBObject* self, const char* key, bool* val) {
    if(!self) return false;

    const RDKBObject* v = _rd_kbobject_seek(self, key);
    return rd_kbobject_to_bool(v, val);
}

bool rd_kbobject_get_int(const RDKBObject* self, const char* key, i64* val) {
    if(!self) return false;

    const RDKBObject* v = _rd_kbobject_seek(self, key);
    return rd_kbobject_to_int(v, val);
}

bool rd_kbobject_get_float(const RDKBObject* self, const char* key,
                           double* val) {
    if(!self) return false;

    const RDKBObject* v = _rd_kbobject_seek(self, key);
    return rd_kbobject_to_float(v, val);
}

bool rd_kbobject_get_time(const RDKBObject* self, const char* key,
                          RDKBTime* val) {
    if(!self) return false;

    const RDKBObject* v = _rd_kbobject_seek(self, key);
    return rd_kbobject_to_time(v, val);
}

bool rd_kbobject_get_date(const RDKBObject* self, const char* key,
                          RDKBDate* val) {
    if(!self) return false;

    const RDKBObject* v = _rd_kbobject_seek(self, key);
    return rd_kbobject_to_date(v, val);
}

bool rd_kbobject_get_datetime(const RDKBObject* self, const char* key,
                              RDKBDateTime* val) {
    if(!self) return false;

    const RDKBObject* v = _rd_kbobject_seek(self, key);
    return rd_kbobject_to_datetime(v, val);
}

const RDKBObject* rd_kbobject_get_table(const RDKBObject* self,
                                        const char* key) {
    if(!self) return NULL;

    const RDKBObject* v = _rd_kbobject_seek(self, key);
    if(!v || rd_kbobject_get_kind(v) != RD_KB_TABLE) return NULL;
    return v;
}

const RDKBObject* rd_kbobject_get_array(const RDKBObject* self,
                                        const char* key) {
    if(!self) return NULL;

    const RDKBObject* v = _rd_kbobject_seek(self, key);
    if(!v || rd_kbobject_get_kind(v) != RD_KB_ARRAY) return NULL;
    return v;
}

const RDKBObject* rd_kbobject_array_at(const RDKBObject* self, usize idx) {
    if(!self || rd_kbobject_get_kind(self) != RD_KB_ARRAY) return NULL;

    if(idx < rd_kbobject_get_length(self)) {
        const toml_datum_t* datum = rd_i_kb_to_datum(self);
        return rd_i_kb_from_datum(&datum->u.arr.elem[idx]);
    }

    return NULL;
}

const char* rd_kbobject_key_at(const RDKBObject* self, usize idx) {
    if(!self || rd_kbobject_get_kind(self) != RD_KB_TABLE) return NULL;

    if(idx < rd_kbobject_get_length(self)) {
        const toml_datum_t* datum = rd_i_kb_to_datum(self);
        return datum->u.tab.key[idx];
    }

    return NULL;
}

const RDKBObject* rd_kbobject_value_at(const RDKBObject* self, usize idx) {
    if(!self || rd_kbobject_get_kind(self) != RD_KB_TABLE) return NULL;

    if(idx < rd_kbobject_get_length(self)) {
        const toml_datum_t* datum = rd_i_kb_to_datum(self);
        return rd_i_kb_from_datum(&datum->u.tab.value[idx]);
    }

    return NULL;
}

const char* rd_kbobject_to_str(const RDKBObject* self) {
    if(!self || rd_kbobject_get_kind(self) != RD_KB_STR) return NULL;
    return rd_i_kb_to_datum(self)->u.s;
}

bool rd_kbobject_to_bool(const RDKBObject* self, bool* val) {
    if(!self || rd_kbobject_get_kind(self) != RD_KB_BOOL) return NULL;
    if(val) *val = rd_i_kb_to_datum(self)->u.boolean;
    return true;
}

bool rd_kbobject_to_int(const RDKBObject* self, i64* val) {
    if(!self || rd_kbobject_get_kind(self) != RD_KB_INT) return NULL;
    if(val) *val = rd_i_kb_to_datum(self)->u.int64;
    return true;
}

bool rd_kbobject_to_float(const RDKBObject* self, double* val) {
    if(!self || rd_kbobject_get_kind(self) != RD_KB_FLOAT) return NULL;
    if(val) *val = rd_i_kb_to_datum(self)->u.fp64;
    return true;
}

bool rd_kbobject_to_time(const RDKBObject* self, RDKBTime* val) {
    if(!self || rd_kbobject_get_kind(self) != RD_KB_TIME) return NULL;

    if(val) {
        const toml_datum_t* datum = rd_i_kb_to_datum(self);

        *val = (RDKBTime){
            .hour = datum->u.ts.hour,
            .minute = datum->u.ts.minute,
            .second = datum->u.ts.second,
            .usec = datum->u.ts.usec,
        };
    }

    return true;
}

bool rd_kbobject_to_date(const RDKBObject* self, RDKBDate* val) {
    if(!self || rd_kbobject_get_kind(self) != RD_KB_DATE) return NULL;

    if(val) {
        const toml_datum_t* datum = rd_i_kb_to_datum(self);

        *val = (RDKBDate){
            .year = datum->u.ts.year,
            .month = datum->u.ts.month,
            .day = datum->u.ts.day,
        };
    }

    return true;
}

bool rd_kbobject_to_datetime(const RDKBObject* self, RDKBDateTime* val) {
    if(!self) return false;

    const toml_datum_t* datum = rd_i_kb_to_datum(self);
    bool ok = datum->type == TOML_DATETIME || datum->type == TOML_DATETIMETZ;

    if(ok && val) {
        *val = (RDKBDateTime){
            .date =
                {
                    .year = datum->u.ts.year,
                    .month = datum->u.ts.month,
                    .day = datum->u.ts.day,
                },
            .time =
                {
                    .hour = datum->u.ts.hour,
                    .minute = datum->u.ts.minute,
                    .second = datum->u.ts.second,
                    .usec = datum->u.ts.usec,
                },
            .tz = datum->type == TOML_DATETIMETZ ? datum->u.ts.tz : 0,
        };
    }

    return ok;
}
