#pragma once

#include <redasm/support/logging.h>

#if defined(NDEBUG)
#define LOG_DEBUG(fmt, ...)
#else
#define LOG_DEBUG(fmt, ...) rd_log(RD_LOG_DEBUG, __func__, fmt, ##__VA_ARGS__)
#endif

#define LOG_INFO(fmt, ...) rd_log(RD_LOG_INFO, __func__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) rd_log(RD_LOG_WARN, __func__, fmt, ##__VA_ARGS__)
#define LOG_FAIL(fmt, ...) rd_log(RD_LOG_FAIL, __func__, fmt, ##__VA_ARGS__)
