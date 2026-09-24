#include "path.h"
#include "core/state.h"
#include "support/error.h"
#include <redasm/allocator.h>
#include <redasm/support/utils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// clang-format off
#if defined(_WIN32)
#include <windows.h>
#include <io.h> // _access
#define access _access
#define W_OK 2
#else
#include <unistd.h> // access
#endif
// clang-format on

static const char* _rd_path_get_tmppath(void) {
#if defined(_WIN32)
    static char tmp[MAX_PATH];
    DWORD len = GetTempPath(MAX_PATH, tmp);
    if(len == 0 || len > MAX_PATH) return NULL;
    return tmp;
#elif defined(__unix__) || defined(__APPLE__) || defined(__HAIKU__)
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

static bool _rd_path_kind_is(const char* path, int mask) {
    if(!path) return false;

    struct stat st;
    if(stat(path, &st) != 0) return false;

    return ((int)st.st_mode & S_IFMT) == mask;
}

bool rd_i_path_is_writable(const char* path) {
    if(!path) return false;

    if(rd_path_exists(path)) return access(path, W_OK) == 0;

    const char* dir = rd_path_dirname(path);
    if(!dir) return false;

    bool ok = access(dir, W_OK) == 0;
    return ok;
}

char* rd_i_path_tmppath(const char* suffix) {
    if(!suffix) return NULL;

    const char* tmpdir = _rd_path_get_tmppath();
    panic_if(!tmpdir, "cannot get temporary path");

    return rd_strdup(rd_path_join(tmpdir, suffix));
}

char* rd_i_path_unique_tmppath(const char* suffix) {
    char* tmppath = rd_i_path_tmppath(suffix);
    if(!tmppath) return NULL;
    if(!rd_path_exists(tmppath)) return tmppath;

    const char* ext = rd_path_ext(tmppath);
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
        if(!rd_path_exists(p)) break;
    }

    rd_free(tmppath);
    return p;
}

bool rd_path_exists(const char* filepath) {
    if(!filepath) return false;

    struct stat st;
    return stat(filepath, &st) == 0;
}

bool rd_path_isfile(const char* path) {
    return _rd_path_kind_is(path, S_IFREG);
}

bool rd_path_isdir(const char* path) { return _rd_path_kind_is(path, S_IFDIR); }

const char* rd_path_dirname(const char* filepath) {
    if(!filepath) return NULL;

    const char* lsep = strrchr(filepath, '/');

#if defined(_WIN32)
    const char* lsep_win = strrchr(filepath, '\\');
    if(lsep_win > lsep) lsep = lsep_win;
#endif

    str_clear(&rd_i_state.path_dirname_buf);

    if(lsep) {
        usize len = (usize)(lsep - filepath);
        str_append_n(&rd_i_state.path_dirname_buf, filepath, len);
    }
    else
        str_push(&rd_i_state.path_dirname_buf, '.');

    return rd_i_state.path_dirname_buf.data;
}

const char* rd_path_filename(const char* filepath) {
    if(!filepath) return NULL;

    const char* lsep = strrchr(filepath, '/');

#if defined(_WIN32)
    const char* lsep_win = strrchr(filepath, '\\');
    if(lsep_win > lsep) lsep = lsep_win;
#endif

    return lsep ? lsep + 1 : filepath;
}

const char* rd_path_ext(const char* filepath) {
    const char* fname = rd_path_filename(filepath);
    if(!fname) return NULL;

    const char* ldot =
        strrchr(fname, '.'); // search filename only, not full path

    // Has extension if dot exists and isn't at the start (avoid
    // ".gitignore")
    if(ldot && ldot != fname) return ldot + 1;

    return ""; // no extension
}

const char* rd_path_stem(const char* filepath) {
    const char* filename = rd_path_filename(filepath);
    const char* fileext = rd_path_ext(filepath);

    str_clear(&rd_i_state.path_stem_buf);

    if(!fileext || !(*fileext) || filename == fileext) {
        str_append(&rd_i_state.path_stem_buf, filename);
        return rd_i_state.path_stem_buf.data;
    }

    usize n = (usize)(fileext - filename - 1);
    str_append_n(&rd_i_state.path_stem_buf, filename, n);
    return rd_i_state.path_stem_buf.data;
}

const char* rd_path_join(const char* a, const char* b) {
    if(!a || !b) return NULL;

    if(!*a) {
        str_clear(&rd_i_state.path_join_buf);
        str_append(&rd_i_state.path_join_buf, b);
        return rd_i_state.path_join_buf.data;
    }

    if(!*b) {
        str_clear(&rd_i_state.path_join_buf);
        str_append(&rd_i_state.path_join_buf, a);
        return rd_i_state.path_join_buf.data;
    }

    usize a_len = strlen(a);
    usize b_len = strlen(b);

    bool a_has_sep = a[a_len - 1] == RD_PATH_SEP;
    bool b_has_sep = b[0] == RD_PATH_SEP;

    // skip b's leading separator if a already ends with one, avoid "//"
    if(a_has_sep && b_has_sep) {
        b++;
        b_len--;
    }

    str_clear(&rd_i_state.path_join_buf);
    str_append_n(&rd_i_state.path_join_buf, a, a_len);

    if(!a_has_sep && !b_has_sep)
        str_push(&rd_i_state.path_join_buf, RD_PATH_SEP);

    str_append_n(&rd_i_state.path_join_buf, b, b_len);
    return rd_i_state.path_join_buf.data;
}
