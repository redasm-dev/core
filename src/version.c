#include <redasm/version.h>

#if !defined(RD_VERSION)
#define RD_VERSION "unknown"
#endif

#if !defined(RD_BUILD_VERSION)
#define RD_BUILD_VERSION "unknown"
#endif

const char* rd_version(void) { return RD_VERSION; }
const char* rd_build_version(void) { return RD_BUILD_VERSION; }
