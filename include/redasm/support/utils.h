#pragma once

#include <redasm/config.h>

RD_API const char* rd_format(const char* fmt, ...);
RD_API char* rd_strdup(const char* s);
RD_API int rd_stricmp(const char* a, const char* b);
RD_API void rd_free(void* p);
