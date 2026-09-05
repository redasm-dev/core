#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#define RD_EXTERN_C extern "C"
#else
#define RD_EXTERN_C
#endif

#if defined _MSC_VER // Defined by Visual Studio
#define RD_IMPORT RD_EXTERN_C __declspec(dllimport)
#define RD_API RD_EXTERN_C __declspec(dllexport)
#else
#if __GNUC__ >= 4 // Defined by GNU C Compiler. Also for C++
#define RD_IMPORT RD_EXTERN_C __attribute__((visibility("default")))
#define RD_API RD_EXTERN_C __attribute__((visibility("default")))
#else
#define RD_IMPORT RD_EXTERN_C
#define RD_API RD_EXTERN_C
#endif
#endif

#if defined(__GNUC__) || defined(__clang__)
#define RD_PRINTF_CHECK(fmt_idx, args_idx)                                     \
    __attribute__((format(printf, fmt_idx, args_idx)))
#define RD_MSVC_CHECK
#elif defined(_MSC_VER)
#define RD_PRINTF_CHECK(fmt_idx, args_idx)
#include <sal.h>
#define RD_MSVC_CHECK _Printf_format_string_
#else
#define RD_PRINTF_CHECK(fmt_idx, args_idx)
#define RD_MSVC_CHECK
#endif

#if defined(__cplusplus)
#define RD_CAST(T, x) static_cast<T>(x)
#else
#define RD_CAST(T, x) ((T)(x))
#endif

// NOLINTBEGIN
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef ptrdiff_t isize;
typedef size_t usize;

typedef uintptr_t uptr;
typedef intptr_t iptr;
// NOLINTEND

typedef u64 RDAddress;
typedef u64 RDOffset;
