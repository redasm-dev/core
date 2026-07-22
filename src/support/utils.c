#include "utils.h"
#include "core/state.h"
#include "io/buffer.h"
#include "support/containers.h"
#include "support/error.h"
#include "support/scratch.h"
#include <ctype.h>
#include <redasm/allocator.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <io.h> // _access
#define access _access
#define W_OK 2
#else
#include <unistd.h> // access
#endif

#define _rd_rol_impl(val, n, bits)                                             \
    (((val) << ((n) & ((bits) - 1))) |                                         \
     ((val) >> (((bits) - (n)) & ((bits) - 1))))

#define _rd_ror_impl(val, n, bits)                                             \
    (((val) >> ((n) & ((bits) - 1))) |                                         \
     ((val) << (((bits) - (n)) & ((bits) - 1))))

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

int rd_i_address_cmp_pred(const void* a, const void* b) {
    RDAddress addr1 = *(RDAddress*)a;
    RDAddress addr2 = *(RDAddress*)b;
    if(addr1 < addr2) return -1;
    if(addr1 > addr2) return 1;
    return 0;
}

int rd_i_address_kcmp_pred(const void* key, const void* item) {
    RDAddress k = *(RDAddress*)key;
    RDAddress i = *(RDAddress*)item;
    if(k < i) return -1;
    if(k > i) return 1;
    return 0;
}

int rd_i_strcmp_pred(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

int rd_i_strcmp_intern_pred(const void* a, const void* b) {
    return (int)((*(const char**)a) - (*(const char**)b));
}

int rd_i_strcmp_key_pred(const void* key, const void* s) {
    return strcmp((const char*)key, *(const char**)s);
}

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

RDWriteFileResult rd_i_writefile(const char* filepath, const char* data,
                                 usize n) {
    FILE* fp = fopen(filepath, "wb");
    if(!fp) return RD_WRITEFILE_FAIL;

    usize written = fwrite(data, sizeof(char), n, fp);
    fclose(fp);
    if(written == n) return RD_WRITEFILE_OK;

    remove(filepath); // remove stale file
    RD_LOG_FAIL("written %zu bytes (expected %zu) file deleted", written, n);
    return RD_WRITEFILE_TRUNC;
}

bool rd_i_file_exists(const char* filepath) {
    assert(filepath);

    struct stat st;
    return stat(filepath, &st) == 0;
}

bool rd_i_path_is_writable(const char* path) {
    assert(path);

    if(rd_i_file_exists(path)) return access(path, W_OK) == 0;

    char* dir = rd_i_get_file_path(path);
    if(!dir) return false;

    bool ok = access(dir, W_OK) == 0;
    rd_free(dir);
    return ok;
}

const char* rd_i_strip_prefix(const char* s) {
    if(!s) return s;

    const char* res = NULL;

    if(strstr(s, "loc_") == s)
        res = s + sizeof("loc_") - 1;
    else if(strstr(s, "sub_") == s)
        res = s + sizeof("sub_") - 1;

    if(res) {
        if(*(res + 1) != 0) return res;
        return s;
    }

    const char* split = strrchr(s, '_');
    if(!split) return s; // no underscores found

    const char* p = s;

    // left part: only characters numbers and underscores allowed
    while(p < split) {
        if(*p != '_' && !islower((int)*p) && !isdigit((int)*p)) return s;
        p++;
    }

    p = split + 1;
    if(*p == 0) return s; // prefix only string

    return p;
}

const char* rd_i_tolower(char* s) {
    for(char* p = s; *p; p++)
        *p = (char)tolower((int)*p);

    return s;
}

u64 rd_align_up(u64 val, u64 align) {
    u64 diff = val % align;
    return diff ? (val + (align - diff)) : val;
}

char* rd_strdup(const char* s) {
    if(!s) return NULL;

    size_t n = strlen(s);
    char* d = rd_alloc(n + 1);
    d[n] = 0;
    return memcpy(d, s, n);
}

char* rd_stristr(const char* haystack, const char* needle) {
    if(!haystack || !needle) return NULL;
    if(!(*needle)) return (char*)haystack;

    for(; *haystack; haystack++) {
        if(tolower((unsigned char)*haystack) != tolower((unsigned char)*needle))
            continue;

        const char *h = haystack, *n = needle;

        while(*h && *n &&
              tolower((unsigned char)*h) == tolower((unsigned char)*n))
            h++, n++;

        if(!*n) return (char*)haystack;
    }

    return NULL;
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

int rd_strnicmp(const char* a, const char* b, int n) {
    while(n-- && *a && *b) {
        int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
        if(d) return d;
        a++;
        b++;
    }
    return n <= 0 ? 0 : tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

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

    const char* ldot =
        strrchr(fname, '.'); // search filename only, not full path

    // Has extension if dot exists and isn't at the start (avoid
    // ".gitignore")
    if(ldot && ldot != fname) return ldot + 1;

    return filepath + strlen(filepath); // no extension
}

char* rd_i_get_file_path(const char* filepath) {
    if(!filepath) return NULL;

    const char* lsep = strrchr(filepath, '/');

#if defined(_WIN32)
    const char* lsep_win = strrchr(filepath, '\\');
    if(lsep_win > lsep) lsep = lsep_win;
#endif

    if(!lsep) {
        char* dot = rd_alloc(2);
        dot[0] = '.';
        dot[1] = '\0';
        return dot;
    }

    usize len = (usize)(lsep - filepath);
    char* out = rd_alloc(len + 1);
    memcpy(out, filepath, len);
    out[len] = '\0';
    return out;
}

char* rd_i_get_file_stem(const char* filepath) {
    const char* filename = rd_i_get_file_name(filepath);
    const char* fileext = rd_i_get_file_ext(filepath);

    // handle names without extension
    if(!fileext || !(*fileext)) return rd_strdup(filename);

    // handle names like ".gitignore"
    if(filename == fileext) return rd_strdup(fileext);

    ptrdiff_t n = (fileext - filename - 1);
    char* stem = rd_alloc((usize)n + 1);
    memcpy(stem, filename, (usize)n);
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

    char* p = rd_alloc(tmpdir_len + suffix_len + 2);
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
    if(!rd_i_file_exists(tmppath)) return tmppath;

    const char* ext = rd_i_get_file_ext(tmppath);
    size_t baselen, extlen;

    if(ext) {
        baselen = (usize)(ext - tmppath - 1);
        extlen = strlen(ext);
    }
    else {
        ext = "";
        baselen = strlen(tmppath);
        extlen = 0;
    }

    // +32 covers " (n)" with generous room for the integer digits
    size_t buflen = baselen + extlen + 32;
    char* p = rd_alloc(buflen);

    for(unsigned int i = 1;; i++) {
        snprintf(p, buflen, "%.*s_%d.%s", (int)baselen, tmppath, i, ext);
        if(!rd_i_file_exists(p)) break;
    }

    rd_free(tmppath);
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

    // Latin-1 printable: convert to UTF-8 directly
    if(c >= 0xA0 && c <= 0xFF) {
        static char utf8_buf[3];
        utf8_buf[0] = (char)(0xC0 | (c >> 6));   // 0xC2 or 0xC3
        utf8_buf[1] = (char)(0x80 | (c & 0x3F)); // continuation byte
        utf8_buf[2] = '\0';
        return utf8_buf;
    }

    static char buf[8]; // \uXXXX + null
    snprintf(buf, sizeof(buf), "\\u%04x", c);
    return buf;
}

int rd_i_utf8_decode(const char* s, u32* cp) {
    u8 b = (u8)*s;

    if(b < 0x80) { // ASCII: 0xxxxxxx
        *cp = b;
        return 1;
    }

    if(b < 0xE0) { // 2 bytes: 110xxxxx 10xxxxxx
        *cp = (u32)((b & 0x1F) << 6) | (u32)((u8)s[1] & 0x3F);
        return 2;
    }

    if(b < 0xF0) { // 3 bytes: 1110xxxx 10xxxxxx 10xxxxxx
        *cp = (u32)((b & 0x0F) << 12) | (u32)(((u8)s[1] & 0x3F) << 6) |
              (u32)((u8)s[2] & 0x3F);
        return 3;
    }

    // 4 bytes: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    *cp = (u32)((b & 0x07) << 18) | (u32)(((u8)s[1] & 0x3F) << 12) |
          (u32)(((u8)s[2] & 0x3F) << 6) | (u32)((u8)s[3] & 0x3F);
    return 4;
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
    uintmax_t u = is_neg ? ~(uintmax_t)v + 1 : (uintmax_t)v;

    // Build digits right-to-left
    unsigned int digit_start = (unsigned int)c;

    do {
        out[--c] = DIGITS[u % p->base];
        u /= p->base;
    } while(u > 0);

    unsigned int digit_count = digit_start - (unsigned int)c;

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
    if(p->with_sign) {
        if(is_neg)
            out[--c] = '-';
        else if(p->with_sign)
            out[--c] = '+';
    }

    return &out[c];
}

int rd_i_cp_push_utf8(RDCharVect* v, u32 cp) {
    if(cp <= 0x7F) {
        vect_push(v, (char)cp);
        return 1;
    }

    if(cp <= 0x7FF) {
        vect_push(v, (char)(0xC0 | (cp >> 6)));
        vect_push(v, (char)(0x80 | (cp & 0x3F)));
        return 2;
    }

    if(cp <= 0xFFFF) {
        vect_push(v, (char)(0xE0 | (cp >> 12)));
        vect_push(v, (char)(0x80 | ((cp >> 6) & 0x3F)));
        vect_push(v, (char)(0x80 | (cp & 0x3F)));
        return 3;
    }

    vect_push(v, (char)(0xF0 | (cp >> 18)));
    vect_push(v, (char)(0x80 | ((cp >> 12) & 0x3F)));
    vect_push(v, (char)(0x80 | ((cp >> 6) & 0x3F)));
    vect_push(v, (char)(0x80 | (cp & 0x3F)));
    return 4;
}

char* rd_i_vformat(RDCharVect* buf, const char* fmt, va_list args) {
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(buf->data, vect_capacity(buf), fmt, args_copy);
    va_end(args_copy);

    if(len >= (int)vect_capacity(buf)) {
        vect_reserve(buf, (usize)len + 1);
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

const char* rd_format_to(RDScratchBuffer* buf, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    const char* result = rd_i_vformat(&buf->impl, fmt, args);
    va_end(args);
    return result;
}

u8 rd_ror8(u8 val, u8 n) { return (u8)_rd_ror_impl(val, n, 8); }
u16 rd_ror16(u16 val, u16 n) { return (u16)_rd_ror_impl(val, n, 16); }
u32 rd_ror32(u32 val, u32 n) { return _rd_ror_impl(val, n, 32); }
u64 rd_ror64(u64 val, u64 n) { return _rd_ror_impl(val, n, 64); }
u8 rd_rol8(u8 val, u8 n) { return (u8)_rd_rol_impl(val, n, 8); }
u16 rd_rol16(u16 val, u16 n) { return (u16)_rd_rol_impl(val, n, 16); }
u32 rd_rol32(u32 val, u32 n) { return _rd_rol_impl(val, n, 32); }
u64 rd_rol64(u64 val, u64 n) { return _rd_rol_impl(val, n, 64); }
