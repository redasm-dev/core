#pragma once

#include <redasm/config.h>
#include <stdarg.h>

typedef struct RDByteBuffer RDByteBuffer;

typedef struct RDCharVect {
    char* data;
    usize length;
    usize capacity;
} RDCharVect;

typedef struct RDBaseParams {
    unsigned int base;
    unsigned int fill;
    bool with_prefix;
    bool with_sign;
} RDBaseParams;

RDByteBuffer* rd_i_readfile(const char* filepath);
const char* rd_i_tolower(char* s);
const char* rd_i_get_file_name(const char* filepath);
const char* rd_i_get_file_ext(const char* filepath);
char* rd_i_get_file_stem(const char* filepath);
char* rd_i_get_temp_path(const char* suffix);
char* rd_i_get_unique_temp_path(const char* suffix);
const char* rd_i_escape_char(char c, bool isstr);
const char* rd_i_escape_char16(u16 c, bool isstr);
const char* rd_i_to_base(i64 v, const RDBaseParams* p);
char* rd_i_vformat(RDCharVect* buf, const char* fmt, va_list args);
char* rd_i_format(RDCharVect* buf, const char* fmt, ...);

static inline const char* rd_i_to_dec(i64 v) {
    return rd_i_to_base(v, &(RDBaseParams){
                               .base = 10,
                               .with_sign = false,
                           });
}

static inline const char* rd_i_to_hex(i64 v, int size) {
    return rd_i_to_base(v, &(RDBaseParams){
                               .base = 16,
                               .fill = size * 2,
                               .with_prefix = false,
                           });
}

static inline usize rd_i_min(usize a, usize b) { return a < b ? a : b; }
static inline usize rd_i_max(usize a, usize b) { return a > b ? a : b; }
