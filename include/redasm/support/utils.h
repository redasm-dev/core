#pragma once

#include <redasm/config.h>

RD_API const char* rd_format(const char* fmt, ...);
RD_API u64 rd_align_up(u64 val, u64 align);
RD_API char* rd_strdup(const char* s);
RD_API char* rd_stristr(const char* haystack, const char* needle);
RD_API int rd_stricmp(const char* a, const char* b);
RD_API int rd_strnicmp(const char* a, const char* b, int n);
RD_API u8 rd_ror8(u8 val, u8 n);
RD_API u16 rd_ror16(u16 val, u16 n);
RD_API u32 rd_ror32(u32 val, u32 n);
RD_API u64 rd_ror64(u64 val, u64 n);
RD_API u8 rd_rol8(u8 val, u8 n);
RD_API u16 rd_rol16(u16 val, u16 n);
RD_API u32 rd_rol32(u32 val, u32 n);
RD_API u64 rd_rol64(u64 val, u64 n);
