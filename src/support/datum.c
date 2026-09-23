#include "datum.h"
#include "core/state.h"
#include <string.h>

// we need pointers, do not use toml_get
static const toml_datum_t* _rd_datum_get(const toml_datum_t* datum,
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

// we need pointers, do not use toml_seek
static const RDDatum* _rd_datum_seek(const RDDatum* self, const char* key) {
    const toml_datum_t* datum = rd_i_datum_to_handle(self);
    if(!self || datum->type != TOML_TABLE || !key) return NULL;

    str_clear(&rd_i_state.toml_key_buf);
    str_append(&rd_i_state.toml_key_buf, key);

    char* p = rd_i_state.toml_key_buf.data;
    const toml_datum_t* cur = datum;

    while(cur && datum->type == TOML_TABLE) {
        char* q = strchr(p, '.');

        if(q) {
            *q = 0;
            cur = _rd_datum_get(cur, p);
            p = q + 1;
            continue;
        }

        return rd_i_datum_from_handle(_rd_datum_get(cur, p));
    }

    return NULL;
}

usize rd_datum_get_length(const RDDatum* self) {
    const toml_datum_t* datum = rd_i_datum_to_handle(self);

    if(datum) {
        if(datum->type == TOML_ARRAY) return (usize)datum->u.arr.size;
        if(datum->type == TOML_TABLE) return (usize)datum->u.tab.size;
    }

    return 0;
}

RDDatumKind rd_datum_get_kind(const RDDatum* self) {
    const toml_datum_t* datum = rd_i_datum_to_handle(self);

    if(datum) {
        switch(datum->type) {
            case TOML_UNKNOWN: return RD_DATUM_UNKNOWN;
            case TOML_STRING: return RD_DATUM_STR;
            case TOML_INT64: return RD_DATUM_INT;
            case TOML_FP64: return RD_DATUM_FLOAT;
            case TOML_BOOLEAN: return RD_DATUM_BOOL;
            case TOML_DATE: return RD_DATUM_DATE;
            case TOML_ARRAY: return RD_DATUM_ARRAY;
            case TOML_TABLE: return RD_DATUM_TABLE;
            case TOML_TIME: return RD_DATUM_TIME;

            case TOML_DATETIME:
            case TOML_DATETIMETZ: return RD_DATUM_DATETIME;

            default: break;
        }
    }

    return RD_DATUM_UNKNOWN;
}

const RDDatum* rd_datum_get(const RDDatum* self, const char* key) {
    return self ? _rd_datum_seek(self, key) : NULL;
}

const char* rd_datum_get_str(const RDDatum* self, const char* key) {
    if(!self) return NULL;

    const RDDatum* v = _rd_datum_seek(self, key);
    return rd_datum_to_str(v);
}

bool rd_datum_get_bool(const RDDatum* self, const char* key, bool* val) {
    if(!self) return false;

    const RDDatum* v = _rd_datum_seek(self, key);
    return rd_datum_to_bool(v, val);
}

bool rd_datum_get_int(const RDDatum* self, const char* key, i64* val) {
    if(!self) return false;

    const RDDatum* v = _rd_datum_seek(self, key);
    return rd_datum_to_int(v, val);
}

bool rd_datum_get_float(const RDDatum* self, const char* key, double* val) {
    if(!self) return false;

    const RDDatum* v = _rd_datum_seek(self, key);
    return rd_datum_to_float(v, val);
}

bool rd_datum_get_time(const RDDatum* self, const char* key, RDDatumTime* val) {
    if(!self) return false;

    const RDDatum* v = _rd_datum_seek(self, key);
    return rd_datum_to_time(v, val);
}

bool rd_datum_get_date(const RDDatum* self, const char* key, RDDatumDate* val) {
    if(!self) return false;

    const RDDatum* v = _rd_datum_seek(self, key);
    return rd_datum_to_date(v, val);
}

bool rd_datum_get_datetime(const RDDatum* self, const char* key,
                           RDDatumDateTime* val) {
    if(!self) return false;

    const RDDatum* v = _rd_datum_seek(self, key);
    return rd_datum_to_datetime(v, val);
}

const RDDatum* rd_datum_get_table(const RDDatum* self, const char* key) {
    if(!self) return NULL;

    const RDDatum* v = _rd_datum_seek(self, key);
    if(!v || rd_datum_get_kind(v) != RD_DATUM_TABLE) return NULL;
    return v;
}

const RDDatum* rd_datum_get_array(const RDDatum* self, const char* key) {
    if(!self) return NULL;

    const RDDatum* v = _rd_datum_seek(self, key);
    if(!v || rd_datum_get_kind(v) != RD_DATUM_ARRAY) return NULL;
    return v;
}

const RDDatum* rd_datum_array_at(const RDDatum* self, usize idx) {
    if(!self || rd_datum_get_kind(self) != RD_DATUM_ARRAY) return NULL;

    if(idx < rd_datum_get_length(self)) {
        const toml_datum_t* datum = rd_i_datum_to_handle(self);
        return rd_i_datum_from_handle(&datum->u.arr.elem[idx]);
    }

    return NULL;
}

const char* rd_datum_key_at(const RDDatum* self, usize idx) {
    if(!self || rd_datum_get_kind(self) != RD_DATUM_TABLE) return NULL;

    if(idx < rd_datum_get_length(self)) {
        const toml_datum_t* datum = rd_i_datum_to_handle(self);
        return datum->u.tab.key[idx];
    }

    return NULL;
}

const RDDatum* rd_datum_value_at(const RDDatum* self, usize idx) {
    if(!self || rd_datum_get_kind(self) != RD_DATUM_TABLE) return NULL;

    if(idx < rd_datum_get_length(self)) {
        const toml_datum_t* datum = rd_i_datum_to_handle(self);
        return rd_i_datum_from_handle(&datum->u.tab.value[idx]);
    }

    return NULL;
}

const char* rd_datum_to_str(const RDDatum* self) {
    if(!self || rd_datum_get_kind(self) != RD_DATUM_STR) return NULL;
    return rd_i_datum_to_handle(self)->u.s;
}

bool rd_datum_to_bool(const RDDatum* self, bool* val) {
    if(!self || rd_datum_get_kind(self) != RD_DATUM_BOOL) return false;
    if(val) *val = rd_i_datum_to_handle(self)->u.boolean;
    return true;
}

bool rd_datum_to_int(const RDDatum* self, i64* val) {
    if(!self || rd_datum_get_kind(self) != RD_DATUM_INT) return false;
    if(val) *val = rd_i_datum_to_handle(self)->u.int64;
    return true;
}

bool rd_datum_to_float(const RDDatum* self, double* val) {
    if(!self || rd_datum_get_kind(self) != RD_DATUM_FLOAT) return false;
    if(val) *val = rd_i_datum_to_handle(self)->u.fp64;
    return true;
}

bool rd_datum_to_time(const RDDatum* self, RDDatumTime* val) {
    if(!self || rd_datum_get_kind(self) != RD_DATUM_TIME) return false;

    if(val) {
        const toml_datum_t* datum = rd_i_datum_to_handle(self);

        *val = (RDDatumTime){
            .hour = datum->u.ts.hour,
            .minute = datum->u.ts.minute,
            .second = datum->u.ts.second,
            .usec = datum->u.ts.usec,
        };
    }

    return true;
}

bool rd_datum_to_date(const RDDatum* self, RDDatumDate* val) {
    if(!self || rd_datum_get_kind(self) != RD_DATUM_DATE) return false;

    if(val) {
        const toml_datum_t* datum = rd_i_datum_to_handle(self);

        *val = (RDDatumDate){
            .year = datum->u.ts.year,
            .month = datum->u.ts.month,
            .day = datum->u.ts.day,
        };
    }

    return true;
}

bool rd_datum_to_datetime(const RDDatum* self, RDDatumDateTime* val) {
    if(!self) return false;

    const toml_datum_t* datum = rd_i_datum_to_handle(self);
    bool ok = datum->type == TOML_DATETIME || datum->type == TOML_DATETIMETZ;

    if(ok && val) {
        *val = (RDDatumDateTime){
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
