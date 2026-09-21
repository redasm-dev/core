#pragma once

#include <redasm/support/path.h>

#ifdef _WIN32
#define RD_PATH_SEP '\\'
#else
#define RD_PATH_SEP '/'
#endif

typedef struct RDPathVect {
    char** data;
    usize length;
    usize capacity;
} RDPathVect;

bool rd_i_path_is_writable(const char* path);
char* rd_i_path_tmppath(const char* suffix);
char* rd_i_path_unique_tmppath(const char* suffix);
