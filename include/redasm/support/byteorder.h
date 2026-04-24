#pragma once

#include <redasm/config.h>
#include <sys/param.h>

#if defined(__BYTE_ORDER)
#if defined(__BIG_ENDIAN) && (__BYTE_ORDER == __BIG_ENDIAN)
#define RD_IS_LITTLE_ENDIAN 0
#define RD_IS_BIG_ENDIAN 1
#elif defined(__LITTLE_ENDIAN) && (__BYTE_ORDER == __LITTLE_ENDIAN)
#define RD_IS_LITTLE_ENDIAN 1
#define RD_IS_BIG_ENDIAN 0
#else
#error "byteorder: unsupported endianness"
#endif
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
