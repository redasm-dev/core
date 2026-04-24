#include <redasm/support/byteorder.h>
#include <stdint.h>

static inline u16 _rd_swap16(u16 x) { return (u16)((x >> 8) | (x << 8)); }

static inline u32 _rd_swap32(u32 x) {
    return ((x >> 24) & 0x000000FF) | ((x >> 8) & 0x0000FF00) |
           ((x << 8) & 0x00FF0000) | ((x << 24) & 0xFF000000);
}

static inline u64 _rd_swap64(u64 x) {
    return ((x >> 56) & UINT64_C(0x00000000000000FF)) |
           ((x >> 40) & UINT64_C(0x000000000000FF00)) |
           ((x >> 24) & UINT64_C(0x0000000000FF0000)) |
           ((x >> 8) & UINT64_C(0x00000000FF000000)) |
           ((x << 8) & UINT64_C(0x000000FF00000000)) |
           ((x << 24) & UINT64_C(0x0000FF0000000000)) |
           ((x << 40) & UINT64_C(0x00FF000000000000)) |
           ((x << 56) & UINT64_C(0xFF00000000000000));
}

u16 rd_fromle16(u16 x) { return RD_IS_LITTLE_ENDIAN ? x : _rd_swap16(x); }
u32 rd_fromle32(u32 x) { return RD_IS_LITTLE_ENDIAN ? x : _rd_swap32(x); }
u64 rd_fromle64(u64 x) { return RD_IS_LITTLE_ENDIAN ? x : _rd_swap64(x); }

u16 rd_frombe16(u16 x) { return RD_IS_BIG_ENDIAN ? x : _rd_swap16(x); }
u32 rd_frombe32(u32 x) { return RD_IS_BIG_ENDIAN ? x : _rd_swap32(x); }
u64 rd_frombe64(u64 x) { return RD_IS_BIG_ENDIAN ? x : _rd_swap64(x); }

u16 rd_tole16(u16 x) { return RD_IS_BIG_ENDIAN ? _rd_swap16(x) : x; }
u32 rd_tole32(u32 x) { return RD_IS_BIG_ENDIAN ? _rd_swap32(x) : x; }
u64 rd_tole64(u64 x) { return RD_IS_BIG_ENDIAN ? _rd_swap64(x) : x; }

u16 rd_tobe16(u16 x) { return RD_IS_LITTLE_ENDIAN ? _rd_swap16(x) : x; }
u32 rd_tobe32(u32 x) { return RD_IS_LITTLE_ENDIAN ? _rd_swap32(x) : x; }
u64 rd_tobe64(u64 x) { return RD_IS_LITTLE_ENDIAN ? _rd_swap64(x) : x; }
