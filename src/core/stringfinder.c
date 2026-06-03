#include "stringfinder.h"
#include "core/context.h"
#include "io/flagsbuffer.h"
#include "support/containers.h"
#include <string.h>

#define RD_STRING_BASE_CAPACITY 1024
#define RD_MAX_CHARS 256

static char const RD_VALID_CHARS[] = "\t\n\r !\"#$%&'()*+,-./"
                                     "0123456789"
                                     ":;<=>?@"
                                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                     "[\\]^_"
                                     "abcdefghijklmnopqrstuvwxyz"
                                     "{|}";

static bool rd_is_char_valid[RD_MAX_CHARS] = {0};
static bool rd_is_strings_initialized = false;

static const char* const RD_C_FORMATS[] = {
    "%c", "%d",  "%e",  "%E",  "%f",  "%g",  "%G",   "%hi",  "%hu",
    "%i", "%ld", "%li", "%lf", "%Lf", "%lu", "%lli", "%lld", "%llu",
    "%o", "%p",  "%s",  "%u",  "%x",  "%X",  "%n",   "%%",
};

#define RD_N_FORMATS (sizeof(RD_C_FORMATS) / sizeof(*RD_C_FORMATS))

static void _rd_i_strings_init(void) {
    if(!rd_is_strings_initialized) {
        for(const char* c = RD_VALID_CHARS; *c; c++)
            rd_is_char_valid[(int)*c] = true;

        rd_is_strings_initialized = true;
    }
}

static bool _rd_strings_check_format(const char* s, int len) {
    for(unsigned int i = 0; i < RD_N_FORMATS; i++) {
        int fmtlen = (int)strlen(RD_C_FORMATS[i]);
        if(fmtlen == len && !strncmp(s, RD_C_FORMATS[i], (usize)fmtlen))
            return true;
    }

    return false;
}

static void _rd_strings_try_classify(RDContext* ctx, const RDSegmentFull* seg,
                                     usize idx, const char* type,
                                     const RDCharVect* str,
                                     RDCharVect* fmt_buf) {
    int len = (int)vect_length(str) - 1;

    if(!strcmp(type, "char")) { // C-style strings valid only for ASCII
        bool valid =
            (*str->data == '%' && _rd_strings_check_format(str->data, len)) ||
            len >= ctx->min_string;

        if(!valid) return;
    }
    else if(len < ctx->min_string)
        return;

    RDAddress address = rd_i_index2address(seg, idx);

    if(rd_auto_type(ctx, address, type, vect_length(str), RD_TYPE_NONE)) {
        const char* hook = rd_i_format(fmt_buf, "redasm.%s_string_found", type);
        rd_fire_address_hook(ctx, hook, address);
    }
}

static void _rd_find_char_strings(RDContext* ctx, RDCharVect* str,
                                  RDCharVect* fmt_buf) {
    const RDSegmentVect* segments = rd_i_db_get_segments(ctx);

    RDSegmentFull** it;
    vect_each(it, segments) {
        RDSegmentFull* seg = *it;
        RDFlagsBuffer* flags = seg->flags;
        usize startidx = 0;
        vect_clear(str);

        for(usize idx = 0; idx < flags->base.length; idx++) {
            u8 v = 0;
            bool skip = rd_flagsbuffer_has_code(flags, idx) ||
                        rd_flagsbuffer_has_tail(flags, idx) ||
                        !rd_flagsbuffer_get_value(flags, idx, &v);

            if(skip) {
                vect_clear(str);
                continue;
            }

            if(v == 0) {
                vect_push(str, 0);
                _rd_strings_try_classify(ctx, seg, startidx, "char", str,
                                         fmt_buf);
                vect_clear(str);
                continue;
            }

            if(!rd_is_char_valid[(int)v]) {
                vect_clear(str);
                continue;
            }

            if(vect_is_empty(str)) startidx = idx;
            vect_push(str, (char)v);
        }
    }
}

static void _rd_find_char16_strings(RDContext* ctx, RDCharVect* str,
                                    RDCharVect* fmt_buf) {
    const RDSegmentVect* segments = rd_i_db_get_segments(ctx);

    RDSegmentFull** it;
    vect_each(it, segments) {
        RDSegmentFull* seg = *it;
        RDFlagsBuffer* flags = seg->flags;
        usize startidx = 0;
        vect_clear(str);

        if(flags->base.length < sizeof(i16))
            continue; // segment too small for any wide string

        for(usize idx = 0; idx < flags->base.length - 1; idx++) {
            if(idx % 2 != 0) { // alignment gate
                vect_clear(str);
                continue;
            }

            u8 lo = 0, hi = 0;

            bool skip = rd_flagsbuffer_has_code(flags, idx) ||
                        rd_flagsbuffer_has_tail(flags, idx) ||
                        !rd_flagsbuffer_get_value(flags, idx, &lo) ||
                        rd_flagsbuffer_has_code(flags, idx + 1) ||
                        rd_flagsbuffer_has_tail(flags, idx + 1) ||
                        !rd_flagsbuffer_get_value(flags, idx + 1, &hi);

            if(skip) {
                vect_clear(str);
                continue;
            }

            if(lo == 0x00 && hi == 0x00) {
                vect_push(str, 0);
                _rd_strings_try_classify(ctx, seg, startidx, "char16", str,
                                         fmt_buf);
                vect_clear(str);
                continue;
            }

            if(hi != 0x00 || !rd_is_char_valid[(int)lo]) {
                vect_clear(str);
                continue;
            }

            if(vect_is_empty(str)) startidx = idx;
            vect_push(str, (char)lo);
            idx++; // consume hi-byte
        }
    }
}

void rd_i_find_strings(RDContext* ctx) {
    _rd_i_strings_init();

    RDCharVect str = {0};
    RDCharVect fmt_buf = {0};
    vect_reserve(&str, RD_STRING_BASE_CAPACITY);

    _rd_find_char_strings(ctx, &str, &fmt_buf);
    if(ctx->scan_char16) _rd_find_char16_strings(ctx, &str, &fmt_buf);

    vect_destroy(&fmt_buf);
    vect_destroy(&str);
}
