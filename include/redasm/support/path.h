#pragma once

#include <redasm/common.h>

RD_API const char* rd_path_filename(const char* filepath);
RD_API const char* rd_path_ext(const char* filepath);
RD_API const char* rd_path_dirname(const char* filepath);
RD_API const char* rd_path_stem(const char* filepath);
RD_API const char* rd_path_join(const char* a, const char* b);
RD_API bool rd_path_exists(const char* filepath);
RD_API bool rd_path_isfile(const char* path);
RD_API bool rd_path_isdir(const char* path);
