#pragma once

#include <redasm/abi.h>
#include <redasm/config.h>

typedef struct RDVersion {
    int major;
    int minor;
    int patch;
    const char* suffix;
} RDVersion;

RD_API RDVersion rd_version(void);
RD_API const char* rd_version_string(void);
RD_API const char* rd_version_build(void);
RD_API bool rd_version_parse(const char* s, RDVersion* v);
