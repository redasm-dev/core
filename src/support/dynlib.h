#pragma once

#include <redasm/common.h>

typedef struct RDDynLib RDDynLib;

RDDynLib* rd_i_dynlib_open(const char* filepath);
void rd_i_dynlib_close(RDDynLib* self);
const void* rd_i_dynlib_sym(const RDDynLib* self, const char* name);
bool rd_i_dynlib_func(const RDDynLib* self, const char* name, void* fnptr);
void rd_i_dynlib_log_error(void);
