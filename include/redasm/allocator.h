#pragma once

#include <redasm/config.h>

RD_API void* rd_alloc(usize sz);
RD_API void* rd_alloc0(usize n, usize sz);
RD_API void rd_free(void* p);
