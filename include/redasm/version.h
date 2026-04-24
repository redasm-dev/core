#pragma once

#define _RD_XSTR(x) #x
#define _RD_STR(x) _RD_XSTR(x)

#define RD_API_LEVEL 1

#define RD_VERSION_MAJOR 4
#define RD_VERSION_MINOR 0
#define RD_VERSION_REV 0

#define RD_VERSION_STR                                                         \
    _RD_STR(RD_VERSION_MAJOR)                                                  \
    "." _RD_STR(RD_VERSION_MINOR) "." _RD_STR(RD_VERSION_REV)
