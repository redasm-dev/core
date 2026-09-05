#pragma once

#include <redasm/config.h>

#if defined(_MSC_VER)
#define RD_IS_LITTLE_ENDIAN 1
#elif defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
#if defined(__ORDER_PDP_ENDIAN__) && __BYTE_ORDER__ == __ORDER_PDP_ENDIAN__
#error "byteorder: middle-endian hosts are not supported"
#endif
#define RD_IS_LITTLE_ENDIAN (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#else
#error "byteorder: cannot detect host byte order"
#endif

#define RD_IS_BIG_ENDIAN (!RD_IS_LITTLE_ENDIAN)

static inline u16 rd_swap16(u16 x) { return (u16)((x >> 8) | (x << 8)); }

static inline u32 rd_swap32(u32 x) {
    return ((x >> 24) & 0x000000FFU) | ((x >> 8) & 0x0000FF00U) |
           ((x << 8) & 0x00FF0000U) | ((x << 24) & 0xFF000000U);
}

static inline u64 rd_swap64(u64 x) {
    return ((u64)rd_swap32((u32)x) << 32) | rd_swap32((u32)(x >> 32));
}

static inline u16 rd_fromle16(u16 x) {
    return RD_IS_LITTLE_ENDIAN ? x : rd_swap16(x);
}

static inline u32 rd_fromle32(u32 x) {
    return RD_IS_LITTLE_ENDIAN ? x : rd_swap32(x);
}

static inline u64 rd_fromle64(u64 x) {
    return RD_IS_LITTLE_ENDIAN ? x : rd_swap64(x);
}

static inline u16 rd_frombe16(u16 x) {
    return RD_IS_BIG_ENDIAN ? x : rd_swap16(x);
}

static inline u32 rd_frombe32(u32 x) {
    return RD_IS_BIG_ENDIAN ? x : rd_swap32(x);
}

static inline u64 rd_frombe64(u64 x) {
    return RD_IS_BIG_ENDIAN ? x : rd_swap64(x);
}

static inline u16 rd_tole16(u16 x) { return rd_fromle16(x); }
static inline u32 rd_tole32(u32 x) { return rd_fromle32(x); }
static inline u64 rd_tole64(u64 x) { return rd_fromle64(x); }
static inline u16 rd_tobe16(u16 x) { return rd_frombe16(x); }
static inline u32 rd_tobe32(u32 x) { return rd_frombe32(x); }
static inline u64 rd_tobe64(u64 x) { return rd_frombe64(x); }

static inline u16 rd_loadle16(const void* p) {
    const u8* b = RD_CAST(const u8*, p);
    return RD_CAST(u16, b[0] | (b[1] << 8));
}

static inline u32 rd_loadle32(const void* p) {
    const u8* b = RD_CAST(const u8*, p);
    return RD_CAST(u32, b[0]) | (RD_CAST(u32, b[1]) << 8) |
           (RD_CAST(u32, b[2]) << 16) | (RD_CAST(u32, b[3]) << 24);
}

static inline u64 rd_loadle64(const void* p) {
    const u8* b = RD_CAST(const u8*, p);
    return RD_CAST(u64, rd_loadle32(b)) |
           (RD_CAST(u64, rd_loadle32(b + 4)) << 32);
}

static inline u16 rd_loadbe16(const void* p) {
    const u8* b = RD_CAST(const u8*, p);
    return RD_CAST(u16, (b[0] << 8) | b[1]);
}

static inline u32 rd_loadbe32(const void* p) {
    const u8* b = RD_CAST(const u8*, p);
    return (RD_CAST(u32, b[0]) << 24) | (RD_CAST(u32, b[1]) << 16) |
           (RD_CAST(u32, b[2]) << 8) | RD_CAST(u32, b[3]);
}

static inline u64 rd_loadbe64(const void* p) {
    const u8* b = RD_CAST(const u8*, p);
    return (RD_CAST(u64, rd_loadbe32(b)) << 32) |
           RD_CAST(u64, rd_loadbe32(b + 4));
}

static inline u32 rd_loadme32(const void* p) {
    const u8* b = RD_CAST(const u8*, p);
    return (RD_CAST(u32, rd_loadle16(b)) << 16) |
           RD_CAST(u32, rd_loadle16(b + 2));
}

static inline u64 rd_loadme64(const void* p) {
    const u8* b = RD_CAST(const u8*, p);
    return (RD_CAST(u64, rd_loadme32(b)) << 32) |
           RD_CAST(u64, rd_loadme32(b + 4));
}

static inline void rd_storele16(void* p, u16 v) {
    u8* b = RD_CAST(u8*, p);
    b[0] = RD_CAST(u8, v);
    b[1] = RD_CAST(u8, v >> 8);
}

static inline void rd_storele32(void* p, u32 v) {
    u8* b = RD_CAST(u8*, p);
    b[0] = RD_CAST(u8, v);
    b[1] = RD_CAST(u8, v >> 8);
    b[2] = RD_CAST(u8, v >> 16);
    b[3] = RD_CAST(u8, v >> 24);
}

static inline void rd_storele64(void* p, u64 v) {
    u8* b = RD_CAST(u8*, p);
    rd_storele32(b, RD_CAST(u32, v));
    rd_storele32(b + 4, RD_CAST(u32, v >> 32));
}

static inline void rd_storebe16(void* p, u16 v) {
    u8* b = RD_CAST(u8*, p);
    b[0] = RD_CAST(u8, v >> 8);
    b[1] = RD_CAST(u8, v);
}

static inline void rd_storebe32(void* p, u32 v) {
    u8* b = RD_CAST(u8*, p);
    b[0] = RD_CAST(u8, v >> 24);
    b[1] = RD_CAST(u8, v >> 16);
    b[2] = RD_CAST(u8, v >> 8);
    b[3] = RD_CAST(u8, v);
}

static inline void rd_storebe64(void* p, u64 v) {
    u8* b = RD_CAST(u8*, p);
    rd_storebe32(b, RD_CAST(u32, v >> 32));
    rd_storebe32(b + 4, RD_CAST(u32, v));
}

static inline void rd_storeme32(void* p, u32 v) {
    u8* b = RD_CAST(u8*, p);
    rd_storele16(b, RD_CAST(u16, v >> 16));
    rd_storele16(b + 2, RD_CAST(u16, v));
}

static inline void rd_storeme64(void* p, u64 v) {
    u8* b = RD_CAST(u8*, p);
    rd_storeme32(b, RD_CAST(u32, v >> 32));
    rd_storeme32(b + 4, RD_CAST(u32, v));
}
