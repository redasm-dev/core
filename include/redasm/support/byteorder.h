#pragma once

#include <redasm/config.h>

#if defined(__linux__) || defined(__HAIKU__)
#include <endian.h>
#elif defined(_WIN32)
// Windows: no include needed, always little-endian
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <sys/endian.h>
#elif defined(__APPLE__)
#include <machine/endian.h>
#define BYTE_ORDER __DARWIN_BYTE_ORDER
#define LITTLE_ENDIAN __DARWIN_LITTLE_ENDIAN
#define BIG_ENDIAN __DARWIN_BIG_ENDIAN
#else
#error "byteorder: unsupported platform"
#endif

#if defined(_WIN32) ||                                                         \
    (defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN) ||              \
    (defined(_BYTE_ORDER) && _BYTE_ORDER == _LITTLE_ENDIAN) ||                 \
    (defined(BYTE_ORDER) && BYTE_ORDER == LITTLE_ENDIAN)
#define RD_IS_LITTLE_ENDIAN 1
#define RD_IS_BIG_ENDIAN 0
#elif (defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN) ||               \
    (defined(_BYTE_ORDER) && _BYTE_ORDER == _BIG_ENDIAN) ||                    \
    (defined(BYTE_ORDER) && BYTE_ORDER == BIG_ENDIAN)
#define RD_IS_LITTLE_ENDIAN 0
#define RD_IS_BIG_ENDIAN 1
#else
#error "byteorder: cannot detect byte order"
#endif

RD_API u16 rd_fromle16(u16 x);
RD_API u32 rd_fromle32(u32 x);
RD_API u64 rd_fromle64(u64 x);
RD_API u16 rd_frombe16(u16 x);
RD_API u32 rd_frombe32(u32 x);
RD_API u64 rd_frombe64(u64 x);
RD_API u16 rd_tole16(u16 x);
RD_API u32 rd_tole32(u32 x);
RD_API u64 rd_tole64(u64 x);
RD_API u16 rd_tobe16(u16 x);
RD_API u32 rd_tobe32(u32 x);
RD_API u64 rd_tobe64(u64 x);
