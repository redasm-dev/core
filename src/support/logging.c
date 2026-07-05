#include "core/state.h"
#include <redasm/support/logging.h>
#include <stdarg.h>
#include <stdio.h>

void rd_log(RDLogLevel level, const char* tag, const char* fmt, ...) {
    static const char* const LEVEL_STR[] = {
        [RD_LOGLEVEL_DEBUG] = "DEBUG",
        [RD_LOGLEVEL_INFO] = "INFO ",
        [RD_LOGLEVEL_WARN] = "WARN ",
        [RD_LOGLEVEL_FAIL] = "FAIL ",
    };

    const char* ls = level <= RD_LOGLEVEL_FAIL ? LEVEL_STR[level] : "INFO ";

    // 1. always write to stderr: crash-safe, no heap involvement
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[%s](%s): ", ls, tag);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    va_end(args);

    // 2. forward to callback only if registered
    if(rd_i_state.log_callback) {
        va_start(args, fmt);
        const char* msg = rd_i_vformat(&rd_i_state.log_buf, fmt, args);
        va_end(args);
        rd_i_state.log_callback(level, tag, msg, rd_i_state.log_userdata);
    }
}

RD_API void rd_log_to(RDScratchBuffer* buf, RDLogLevel level, const char* tag,
                      const char* fmt, ...) {
    static const char* const LEVEL_STR[] = {
        [RD_LOGLEVEL_DEBUG] = "DEBUG",
        [RD_LOGLEVEL_INFO] = "INFO ",
        [RD_LOGLEVEL_WARN] = "WARN ",
        [RD_LOGLEVEL_FAIL] = "FAIL ",
    };

    const char* ls = level <= RD_LOGLEVEL_FAIL ? LEVEL_STR[level] : "INFO ";

    // 1. always write to stderr: crash-safe, no heap involvement
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[%s](%s): ", ls, tag);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    va_end(args);

    // 2. write to buffer
    va_start(args, fmt);
    rd_i_vformat(&buf->impl, fmt, args);
    va_end(args);

    // 3. forward to callback only if registered
    if(rd_i_state.log_callback) {
        va_start(args, fmt);
        const char* msg = rd_i_vformat(&rd_i_state.log_buf, fmt, args);
        va_end(args);
        rd_i_state.log_callback(level, tag, msg, rd_i_state.log_userdata);
    }
}
