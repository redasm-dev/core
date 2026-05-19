#pragma once

#include <redasm/common.h>
#include <redasm/config.h>

typedef enum {
    RD_KB_UNKNOWN = 0,
    RD_KB_STR,
    RD_KB_INT,
    RD_KB_FLOAT,
    RD_KB_BOOL,
    RD_KB_DATE,
    RD_KB_TIME,
    RD_KB_DATETIME,
    RD_KB_ARRAY,
    RD_KB_TABLE,
} RDKBObjectKind;

typedef struct RDKBTime {
    i16 hour, minute, second;
    i32 usec;
} RDKBTime;

typedef struct RDKBDate {
    i16 year, month, day;
} RDKBDate;

typedef struct RDKBDateTime {
    RDKBDate date;
    RDKBTime time;
    i16 tz;
} RDKBDateTime;

typedef struct RDKBObject RDKBObject;

RD_API const RDKBObject* rd_kb_load(RDContext* ctx, const char* kb);

RD_API usize rd_kbobject_get_length(const RDKBObject* self);
RD_API RDKBObjectKind rd_kbobject_get_kind(const RDKBObject* self);
RD_API const RDKBObject* rd_kbobject_get(const RDKBObject* self,
                                         const char* key);
RD_API const char* rd_kbobject_get_str(const RDKBObject* self, const char* key);
RD_API bool rd_kbobject_get_bool(const RDKBObject* self, const char* key,
                                 bool* val);
RD_API bool rd_kbobject_get_int(const RDKBObject* self, const char* key,
                                i64* val);
RD_API bool rd_kbobject_get_float(const RDKBObject* self, const char* key,
                                  double* val);
RD_API bool rd_kbobject_get_time(const RDKBObject* self, const char* key,
                                 RDKBTime* val);
RD_API bool rd_kbobject_get_date(const RDKBObject* self, const char* key,
                                 RDKBDate* val);
RD_API bool rd_kbobject_get_datetime(const RDKBObject* self, const char* key,
                                     RDKBDateTime* val);
RD_API const RDKBObject* rd_kbobject_get_table(const RDKBObject* self,
                                               const char* key);
RD_API const RDKBObject* rd_kbobject_get_array(const RDKBObject* self,
                                               const char* key);
RD_API const RDKBObject* rd_kbobject_array_at(const RDKBObject* self,
                                              usize idx);
RD_API const char* rd_kbobject_key_at(const RDKBObject* self, usize idx);
RD_API const RDKBObject* rd_kbobject_value_at(const RDKBObject* self,
                                              usize idx);
RD_API const char* rd_kbobject_to_str(const RDKBObject* self);
RD_API bool rd_kbobject_to_bool(const RDKBObject* self, bool* val);
RD_API bool rd_kbobject_to_int(const RDKBObject* self, i64* val);
RD_API bool rd_kbobject_to_float(const RDKBObject* self, double* val);
RD_API bool rd_kbobject_to_time(const RDKBObject* self, RDKBTime* val);
RD_API bool rd_kbobject_to_date(const RDKBObject* self, RDKBDate* val);
RD_API bool rd_kbobject_to_datetime(const RDKBObject* self, RDKBDateTime* val);

#define _RD_KB_CONCAT(a, b) a##b
#define _RD_KB_INDEX(line) _RD_KB_CONCAT(_kb_index_, line)

#define rd_kbobject_each(it, self)                                             \
    for(usize _RD_KB_INDEX(__LINE__) = ((it) = NULL, 0);                       \
        _RD_KB_INDEX(__LINE__) < rd_kbobject_get_length(self) &&               \
        ((it) = rd_kbobject_array_at(self, _RD_KB_INDEX(__LINE__)), (it));     \
        _RD_KB_INDEX(__LINE__)++)

#define rd_kbobject_each_pair(k, v, self)                                      \
    for(usize _RD_KB_INDEX(__LINE__) = ((k) = NULL, (v) = NULL, 0);            \
        _RD_KB_INDEX(__LINE__) < rd_kbobject_get_length(self) &&               \
        ((k) = rd_kbobject_key_at(self, _RD_KB_INDEX(__LINE__)),               \
        (v) = rd_kbobject_value_at(self, _RD_KB_INDEX(__LINE__)), (k) && (v)); \
        _RD_KB_INDEX(__LINE__)++)
