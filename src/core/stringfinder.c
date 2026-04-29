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
        int fmtlen = strlen(RD_C_FORMATS[i]);
        if(fmtlen == len && !strncmp(s, RD_C_FORMATS[i], fmtlen)) return true;
    }

    return false;
}

static void _rd_strings_try_classify(RDContext* ctx, const RDSegmentFull* seg,
                                     usize idx, const RDCharVect* str) {
    usize len = vect_length(str) - 1;

    if(len < ctx->min_string) return;
    if(*str->data == '%' && !_rd_strings_check_format(str->data, len)) return;

    RDAddress addr = rd_i_index2address(seg, idx);
    if(rd_auto_type(ctx, addr, "char", vect_length(str), RD_TYPE_NONE))
        rd_fire_address_hook(ctx, "redasm.string_found", addr);
}

void rd_i_find_strings(RDContext* ctx) {
    _rd_i_strings_init();
    RDCharVect str = {0};
    vect_reserve(&str, RD_STRING_BASE_CAPACITY);

    RDSegmentFull** it;
    vect_each(it, &ctx->segments) {
        RDSegmentFull* seg = *it;
        RDFlagsBuffer* flags = seg->flags;
        usize startidx = 0;
        vect_clear(&str);

        for(usize idx = 0; idx < flags->base.length; idx++) {
            u8 v;
            bool skip = rd_flagsbuffer_has_code(flags, idx) ||
                        rd_flagsbuffer_has_tail(flags, idx) ||
                        !rd_flagsbuffer_get_value(flags, idx, &v);

            if(skip) {
                vect_clear(&str);
                continue;
            }

            if(v == 0) {
                vect_push(&str, 0);
                _rd_strings_try_classify(ctx, seg, startidx, &str);
                vect_clear(&str);
                continue;
            }

            if(!rd_is_char_valid[(int)v]) {
                vect_clear(&str);
                continue;
            }

            if(vect_is_empty(&str)) startidx = idx;
            vect_push(&str, (char)v);
        }
    }

    vect_destroy(&str);
}
