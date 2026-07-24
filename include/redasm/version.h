#pragma once

#include <redasm/config.h>

#define RD_API_LEVEL 1

typedef struct RDVersion {
    int major;
    int minor;
    int rev;
    const char* suffix;
} RDVersion;

RD_API RDVersion rd_version(void);
RD_API const char* rd_version_string(void);
RD_API const char* rd_version_build(void);
