#pragma once

void _log_impl(const char* level, const char* func, const char* fmt, ...);
void _log_impl_l(const char* level, const char* func, int line, const char* fmt,
                 ...);

#if defined(NDEBUG)
#define LOG_TRACE(fmt, ...)
#else
#define LOG_TRACE(fmt, ...)                                                    \
    _log_impl_l("TRACE", __func__, __LINE__, fmt, ##__VA_ARGS__)
#endif

#define LOG_INFO(fmt, ...) _log_impl("INFO ", __func__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) _log_impl("WARN ", __func__, fmt, ##__VA_ARGS__)
#define LOG_FAIL(fmt, ...) _log_impl("FAIL ", __func__, fmt, ##__VA_ARGS__)
