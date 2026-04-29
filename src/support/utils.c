#include "utils.h"
#include "core/state.h"
#include "io/buffer.h"
#include "support/containers.h"
#include "support/error.h"
#include <ctype.h>
#include <redasm/support/utils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#define RD_PATH_SEP '\\'
#else
#define RD_PATH_SEP '/'
#endif

static const char* _rd_get_temp_path(void) {
#if defined(_WIN32)
    static char tmp[MAX_PATH];
    DWORD len = GetTempPath(MAX_PATH, tmp);
    if(len == 0 || len > MAX_PATH) return NULL;
    return tmp;
#elif defined(__unix__) || defined(__APPLE__)
    const char* const CANDIDATES[] = {
        getenv("TMPDIR"), getenv("TEMP"), getenv("TMP"), P_tmpdir, "/tmp",
    };

    const int N_CANDIDATES = sizeof(CANDIDATES) / sizeof(*CANDIDATES);

    for(int i = 0; i < N_CANDIDATES; i++) {
        const char* path = CANDIDATES[i];
        if(path == NULL || path[0] == '\0') continue;

        struct stat st;
        if(stat(path, &st) == 0 && S_ISDIR(st.st_mode)) return path;
    }

    return NULL;
#else
#error "Unsupported temp path implementation"
#endif
}

static const RDBaseParams RD_BASE_DEFAULTS = {
    .base = 10,
    .with_prefix = false,
    .with_sign = true,
    .fill = 0,
};

RDByteBuffer* rd_i_fromdata(const char* bytes, usize n) {
    RDByteBuffer* b = rd_i_buffer_create(n);
    memcpy(b->data, bytes, n);
    return b;
}

RDByteBuffer* rd_i_readfile(const char* filepath) {
    FILE* fp = fopen(filepath, "rb");
    if(!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    RDByteBuffer* b = rd_i_buffer_create((usize)ftell(fp));
    fseek(fp, 0, SEEK_SET);

    fread(b->data, 1, b->base.length, fp);
    fclose(fp);
    return b;
}

const char* rd_i_tolower(char* s) {
    for(char* p = s; *p; p++)
        *p = tolower((unsigned char)*p);

    return s;
}

char* rd_strdup(const char* s) {
    if(!s) return NULL;

    size_t n = strlen(s);
    char* d = malloc(n + 1);
    d[n] = 0;
    return memcpy(d, s, n);
}

int rd_stricmp(const char* a, const char* b) {
    while(*a && *b) {
        int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
        if(d) return d;
        a++;
        b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

void rd_free(void* p) { free(p); }

const char* rd_i_get_file_name(const char* filepath) {
    if(!filepath) return NULL;

    const char* lsep = strrchr(filepath, '/');

#if defined(_WIN32)
    const char* lsep_win = strrchr(filepath, '\\');
    if(lsep_win > lsep) lsep = lsep_win;
#endif

    return lsep ? lsep + 1 : filepath;
}

const char* rd_i_get_file_ext(const char* filepath) {
    const char* fname = rd_i_get_file_name(filepath);
    if(!fname) return NULL;

    const char* ldot = strrchr(filepath, '.');

    // Has extension if dot exists and isn't at the start (avoid ".gitignore")
    if(ldot && ldot != fname) return ldot + 1;

    return NULL;
}

char* rd_i_get_file_stem(const char* filepath) {
    const char* filename = rd_i_get_file_name(filepath);
    const char* fileext = rd_i_get_file_ext(filepath);

    // handle names without extension
    if(!fileext) return rd_strdup(filename);

    // handle names like ".gitignore"
    if(filename == fileext) return rd_strdup(fileext);

    ptrdiff_t n = (fileext - filename - 1);
    char* stem = malloc(n + 1);
    memcpy(stem, filename, n);
    stem[n] = 0;
    return stem;
}

char* rd_i_get_temp_path(const char* suffix) {
    if(!suffix) return NULL;

    const char* tmpdir = _rd_get_temp_path();
    assert(tmpdir && "cannot get temporary path");

    size_t tmpdir_len = strlen(tmpdir);
    size_t suffix_len = strlen(suffix);

    if(tmpdir[tmpdir_len - 1] == RD_PATH_SEP) tmpdir_len--;

    if(*suffix == RD_PATH_SEP) {
        suffix++;
        suffix_len--;
    }

    char* p = malloc(tmpdir_len + suffix_len + 2);
    if(!p) return NULL;

    memcpy(p, tmpdir, tmpdir_len);

    if(suffix_len > 0) {
        p[tmpdir_len] = RD_PATH_SEP;
        memcpy(p + tmpdir_len + 1, suffix, suffix_len);
        p[tmpdir_len + suffix_len + 1] = 0;
    }
    else
        p[tmpdir_len + suffix_len] = 0;

    return p;
}

char* rd_i_get_unique_temp_path(const char* suffix) {
    char* tmppath = rd_i_get_temp_path(suffix);
    if(!tmppath) return NULL;

    struct stat st;
    if(stat(tmppath, &st) != 0) return tmppath;

    const char* ext = rd_i_get_file_ext(tmppath);
    size_t baselen, extlen;

    if(ext) {
        baselen = ext - tmppath - 1;
        extlen = strlen(ext);
    }
    else {
        ext = "";
        baselen = strlen(tmppath);
        extlen = 0;
    }

    // +32 covers " (n)" with generous room for the integer digits
    size_t buflen = baselen + extlen + 32;
    char* p = malloc(buflen);

    for(unsigned int i = 1;; i++) {
        snprintf(p, buflen, "%.*s_%d.%s", (int)baselen, tmppath, i, ext);
        if(stat(p, &st) != 0) break;
    }

    free(tmppath);
    return p;
}

const char* rd_i_escape_char(char c, bool isstr) {
    static char buf[5];

    switch(c) {
        case '\n': return "\\n";
        case '\r': return "\\r";
        case '\t': return "\\t";
        case '\\': return "\\\\";
        case '"': return isstr ? "\\\"" : "\"";
        case '\'': return isstr ? "'" : "\\'";
        case '\0': return "\\0";
        default: break;
    }

    if(isprint(c)) {
        buf[0] = c;
        buf[1] = '\0';
        return buf;
    }

    snprintf(buf, sizeof(buf), "\\x%02x", c);
    return buf;
}

const char* rd_i_escape_char16(u16 c, bool isstr) {
    if(c < 128) return rd_i_escape_char((char)c, isstr);

    static char buf[8]; // \uXXXX + null
    snprintf(buf, sizeof(buf), "\\u%04x", c);
    return buf;
}

const char* rd_i_to_base(i64 v, const RDBaseParams* p) {
    if(!p) p = &RD_BASE_DEFAULTS;
    panic_if(p->base < 2 || p->base > 36, "base %d is not valid", p->base);

    static const char DIGITS[] = "0123456789abcdefghijklmnopqrstuvwxyz";

    // Max bits in isize (64) in base 2 = 64 digits
    // + 1 negative sign + 2 prefix (0x/0b/0o) + 1 NUL = 68
    static char out[68];

    panic_if(p->fill >= sizeof(out), "fill overflow (%d >= %d)", p->fill,
             sizeof(out));

    int c = sizeof(out) - 1;
    out[c] = '\0';

    // Work with unsigned magnitude to handle INTMAX_MIN safely
    bool is_neg = v < 0;
    uintmax_t u = is_neg ? -(uintmax_t)v : (uintmax_t)v;

    // Build digits right-to-left
    unsigned int digit_start = c;

    do {
        out[--c] = DIGITS[u % p->base];
        u /= p->base;
    } while(u > 0);

    unsigned int digit_count = digit_start - c;

    // Zero-padding
    if(p->fill > 0) {
        while(digit_count < p->fill) {
            out[--c] = '0';
            digit_count++;
        }
    }

    // Prefix: 0x, 0b, 0o
    if(p->with_prefix) {
        switch(p->base) {
            case 16:
                out[--c] = 'x';
                out[--c] = '0';
                break;

            case 2:
                out[--c] = 'b';
                out[--c] = '0';
                break;

            case 8:
                out[--c] = 'o';
                out[--c] = '0';
                break;

            default: break; // no prefix for other bases
        }
    }

    // Sign
    if(is_neg)
        out[--c] = '-';
    else if(p->with_sign)
        out[--c] = '+';

    return &out[c];
}

char* rd_i_vformat(RDCharVect* buf, const char* fmt, va_list args) {
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(buf->data, vect_capacity(buf), fmt, args_copy);
    va_end(args_copy);

    if(len >= (int)vect_capacity(buf)) {
        vect_reserve(buf, len + 1);
        va_copy(args_copy, args);
        len = vsnprintf(buf->data, vect_capacity(buf), fmt, args_copy);
        va_end(args_copy);

        assert(len > 0 && len < (int)vect_capacity(buf) &&
               "formatting buffer overflow");
    }

    assert(len >= 0 && "formatting failed");
    return buf->data;
}

char* rd_i_format(RDCharVect* buf, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char* result = rd_i_vformat(buf, fmt, args);
    va_end(args);
    return result;
}

const char* rd_format(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    const char* result = rd_i_vformat(&rd_i_state.fmt_buf, fmt, args);
    va_end(args);
    return result;
}
