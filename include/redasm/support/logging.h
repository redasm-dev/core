#pragma once

#include <redasm/config.h>

typedef enum {
    RD_LOGLEVEL_DEBUG = 0,
    RD_LOGLEVEL_INFO,
    RD_LOGLEVEL_WARN,
    RD_LOGLEVEL_FAIL,
} RDLogLevel;

typedef void (*RDLogCallback)(RDLogLevel level, const char* tag,
                              const char* msg, void* userdata);

RD_API void rd_set_log_callback(RDLogCallback cb, void* userdata);
RD_API void rd_log(RDLogLevel level, const char* tag, const char* fmt, ...);

#if defined(NDEBUG)
#define RD_LOG_DEBUG(fmt, ...)
#else
#define RD_LOG_DEBUG(fmt, ...)                                                    \
    rd_log(RD_LOGLEVEL_DEBUG, __func__, fmt, ##__VA_ARGS__)
#endif

#define RD_LOG_INFO(fmt, ...)                                                     \
    rd_log(RD_LOGLEVEL_INFO, __func__, fmt, ##__VA_ARGS__)
#define RD_LOG_WARN(fmt, ...)                                                     \
    rd_log(RD_LOGLEVEL_WARN, __func__, fmt, ##__VA_ARGS__)
#define RD_LOG_FAIL(fmt, ...)                                                     \
    rd_log(RD_LOGLEVEL_FAIL, __func__, fmt, ##__VA_ARGS__)
