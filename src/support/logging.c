#include "logging.h"
#include <stdarg.h>
#include <stdio.h>

void _log_impl(const char* level, const char* func, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[%s](%s): ", level, func);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    va_end(args);
}

void _log_impl_l(const char* level, const char* func, int line, const char* fmt,
                 ...) {
    va_list args;
    fprintf(stderr, "[%s](%s:%d): ", level, func, line);
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
}
