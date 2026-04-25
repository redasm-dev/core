#include "logging.h"
#include "core/state.h"
#include <stdarg.h>
#include <stdio.h>

void rd_log(RDLogLevel level, const char* tag, const char* fmt, ...) {
    static const char* const LEVEL_STR[] = {
        [RD_LOG_DEBUG] = "DEBUG",
        [RD_LOG_INFO] = "INFO ",
        [RD_LOG_WARN] = "WARN ",
        [RD_LOG_FAIL] = "FAIL ",
    };

    const char* ls = level < RD_LOG_FAIL ? LEVEL_STR[level] : "INFO ";

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
