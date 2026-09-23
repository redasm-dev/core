#pragma once

#include <redasm/config.h>

typedef enum {
    RD_DATUM_UNKNOWN = 0,
    RD_DATUM_STR,
    RD_DATUM_INT,
    RD_DATUM_FLOAT,
    RD_DATUM_BOOL,
    RD_DATUM_DATE,
    RD_DATUM_TIME,
    RD_DATUM_DATETIME,
    RD_DATUM_ARRAY,
    RD_DATUM_TABLE,
} RDDatumKind;

typedef struct RDDatumTime {
    i16 hour, minute, second;
    i32 usec;
} RDDatumTime;

typedef struct RDDatumDate {
    i16 year, month, day;
} RDDatumDate;

typedef struct RDDatumDateTime {
    RDDatumDate date;
    RDDatumTime time;
    i16 tz;
} RDDatumDateTime;

typedef struct RDDatum RDDatum;

RD_API usize rd_datum_get_length(const RDDatum* self);
RD_API RDDatumKind rd_datum_get_kind(const RDDatum* self);
RD_API const RDDatum* rd_datum_get(const RDDatum* self, const char* key);
RD_API const char* rd_datum_get_str(const RDDatum* self, const char* key);
RD_API bool rd_datum_get_bool(const RDDatum* self, const char* key, bool* val);
RD_API bool rd_datum_get_int(const RDDatum* self, const char* key, i64* val);
RD_API bool rd_datum_get_float(const RDDatum* self, const char* key,
                               double* val);
RD_API bool rd_datum_get_time(const RDDatum* self, const char* key,
                              RDDatumTime* val);
RD_API bool rd_datum_get_date(const RDDatum* self, const char* key,
                              RDDatumDate* val);
RD_API bool rd_datum_get_datetime(const RDDatum* self, const char* key,
                                  RDDatumDateTime* val);
RD_API const RDDatum* rd_datum_get_table(const RDDatum* self, const char* key);
RD_API const RDDatum* rd_datum_get_array(const RDDatum* self, const char* key);
RD_API const RDDatum* rd_datum_array_at(const RDDatum* self, usize idx);
RD_API const char* rd_datum_key_at(const RDDatum* self, usize idx);
RD_API const RDDatum* rd_datum_value_at(const RDDatum* self, usize idx);
RD_API const char* rd_datum_to_str(const RDDatum* self);
RD_API bool rd_datum_to_bool(const RDDatum* self, bool* val);
RD_API bool rd_datum_to_int(const RDDatum* self, i64* val);
RD_API bool rd_datum_to_float(const RDDatum* self, double* val);
RD_API bool rd_datum_to_time(const RDDatum* self, RDDatumTime* val);
RD_API bool rd_datum_to_date(const RDDatum* self, RDDatumDate* val);
RD_API bool rd_datum_to_datetime(const RDDatum* self, RDDatumDateTime* val);

#define _RD_DATUM_CONCAT(a, b) a##b
#define _RD_DATUM_INDEX(line) _RD_DATUM_CONCAT(_datum_index_, line)

#define rd_datum_each(it, self)                                                \
    for(usize _RD_DATUM_INDEX(__LINE__) = ((it) = NULL, 0);                    \
        _RD_DATUM_INDEX(__LINE__) < rd_datum_get_length(self) &&               \
        ((it) = rd_datum_array_at(self, _RD_DATUM_INDEX(__LINE__)), (it));     \
        _RD_DATUM_INDEX(__LINE__)++)

#define rd_datum_each_pair(k, v, self)                                         \
    for(usize _RD_DATUM_INDEX(__LINE__) = ((k) = NULL, (v) = NULL, 0);         \
        _RD_DATUM_INDEX(__LINE__) < rd_datum_get_length(self) &&               \
        ((k) = rd_datum_key_at(self, _RD_DATUM_INDEX(__LINE__)),               \
        (v) = rd_datum_value_at(self, _RD_DATUM_INDEX(__LINE__)), (k) && (v)); \
        _RD_DATUM_INDEX(__LINE__)++)
