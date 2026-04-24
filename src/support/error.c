#include "error.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

ERROR_NORET void _panic_impl(const char* file, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[PANIC](%s:%d): ", file, line);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    va_end(args);
    abort();
}
