#include "stringfinder.h"
#include "core/context.h"
#include "io/flagsbuffer.h"
#include "support/containers.h"
#include <math.h>
#include <string.h>

#define RD_STRING_BASE_CAPACITY 1024
#define RD_MIN_STRING_LENGTH 4
#define RD_VOWEL_MIN_LENGTH 8
#define RD_MIN_VOWEL_FREQ 0.15
#define RD_MAX_CHARS 256
#define RD_CHARS_FREQ 0.5 // 50%
#define RD_MIN_ENTROPY 2.0

static char const RD_VALID_CHARS[] = "\t\n\r !\"#$%&'()*+,-./"
                                     "0123456789"
                                     ":;<=>?@"
                                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                     "[\\]^_"
                                     "abcdefghijklmnopqrstuvwxyz"
                                     "{|}";

static const char RD_VOWELS[] = "aeiouAEIOU";

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

static double _rd_strings_entropy(const char* s, int len) {
    int frequency[RD_MAX_CHARS] = {0};

    for(int i = 0; i < len; i++)
        frequency[(int)s[i]]++;

    double e = 0.0;

    for(int i = 0; i < RD_MAX_CHARS; i++) {
        if(frequency[i]) {
            double prob = frequency[i] / (double)len;
            e -= prob * log2(prob);
        }
    }

    return e;
}

static bool _rd_strings_check_format(const char* s, int len) {
    for(unsigned int i = 0; i < RD_N_FORMATS; i++) {
        int fmtlen = strlen(RD_C_FORMATS[i]);
        if(fmtlen == len && !strncmp(s, RD_C_FORMATS[i], fmtlen)) return true;
    }

    return false;
}

static bool _rd_strings_validate(const char* s, int len) {
    if(*s == '%') return _rd_strings_check_format(s, len);
    if(len <= 2) return false;

    const char* e = s + len - 1;

    if(*s == '\'' && *e == '\'') return true;
    if(*s == '\"' && *e == '\"') return true;
    if(*s == '<' && *e == '>') return true;
    if(*s == '(' && *e == ')') return true;
    if(*s == '[' && *e == ']') return true;
    if(*s == '{' && *e == '}') return true;

    return false;
}

static bool _rd_strings_is_gibberish(const char* s, int len) {
    if(_rd_strings_validate(s, len)) return false;

    int chars[RD_MAX_CHARS] = {0};

    for(int i = 0; i < len; i++)
        chars[(int)s[i]]++;

    int nalpha = 0;

    for(int c = 'a'; c <= 'z'; c++)
        nalpha += chars[c];

    for(int c = 'A'; c <= 'Z'; c++)
        nalpha += chars[c];

    if(nalpha / (double)len < RD_CHARS_FREQ) return true;
    if(_rd_strings_entropy(s, len) < RD_MIN_ENTROPY) return true;

    if(len >= RD_VOWEL_MIN_LENGTH && nalpha > 0) {
        int nvowels = 0;
        for(const char* v = RD_VOWELS; *v; v++)
            nvowels += chars[(int)*v];

        if(nvowels / (double)nalpha < RD_MIN_VOWEL_FREQ) return true;
    }

    return false;
}

static void _rd_strings_try_classify(RDContext* ctx, const RDSegmentFull* seg,
                                     usize idx, const RDCharVect* str) {
    usize len = vect_length(str) - 1;

    if(len < RD_MIN_STRING_LENGTH) return;
    if(_rd_strings_is_gibberish(str->data, len)) return;

    RDAddress addr = rd_i_index2address(seg, idx);

    if(rd_i_flagsbuffer_has_type(seg->flags, idx)) {
        RDTypeFull t;
        if(rd_i_db_get_type(ctx, addr, &t)) {
            if(t.confidence >= RD_CONFIDENCE_LIBRARY) return;
            rd_undefine(ctx, addr);
        }
    }

    rd_auto_type(ctx, addr, "char", vect_length(str), RD_TYPE_NONE);
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
            bool skip = rd_i_flagsbuffer_has_code(flags, idx) ||
                        rd_i_flagsbuffer_has_tail(flags, idx) ||
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
