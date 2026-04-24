#pragma once

#include <assert.h>

#if defined(__GNUC__) || defined(__clang__)
#if !defined(unreachable)
#define unreachable() __builtin_unreachable()
#endif
#elif defined(_MSC_VER)
#if !defined(unreachable)
#define unreachable() __assume(0)
#endif
#else
#if !defined(unreachable)
#include <stdlib.h>
#define unreachable() (assert(0 && "unreachable code detected"), abort());
#endif
#endif

#if !defined(ERROR_NORET)
#if defined(__GNUC__) || defined(__clang__)
#define ERROR_NORET __attribute__((noreturn))
#elif defined(_MSC_VER)
#define ERROR_NORET __declspec(noreturn)
#else
#define ERROR_NORET _Noreturn
#endif /* defined(__GNUC__) || defined(__clang__) */
#endif /* !defined(ERROR_NORET) */

#if defined(__GNUC__) || defined(__clang__)
#define ERROR_FILE __FILE_NAME__
#else
#define ERROR_FILE __FILE__
#endif

#if defined(__GNUC__) || defined(__clang__)
#define ERROR_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define ERROR_UNLIKELY(x) (x)
#endif

ERROR_NORET void _panic_impl(const char* file, int line, const char* fmt, ...);

#define panic(fmt, ...) _panic_impl(ERROR_FILE, __LINE__, fmt, ##__VA_ARGS__)

#define panic_if(cond, fmt, ...)                                               \
    do {                                                                       \
        if(ERROR_UNLIKELY(cond)) panic(fmt, ##__VA_ARGS__);                    \
    } while(0)
