#pragma once

#include <redasm/config.h>

typedef enum {
    RD_LOG_DEBUG = 0,
    RD_LOG_INFO,
    RD_LOG_WARN,
    RD_LOG_FAIL,
} RDLogLevel;

typedef void (*RDLogCallback)(RDLogLevel level, const char* tag,
                              const char* msg, void* userdata);

RD_API void rd_set_log_callback(RDLogCallback cb, void* userdata);
RD_API void rd_log(RDLogLevel level, const char* tag, const char* fmt, ...);
