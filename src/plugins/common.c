#include "common.h"
#include <redasm/support/logging.h>
#include <redasm/version.h>

bool rd_i_validate_plugin(u32 level, const char* id, const char* kind) {
    if(!id || !(*id)) {
        RD_LOG_FAIL("invalid %s-plugin id", kind);
        return false;
    }

    if(level != RD_API_LEVEL) {
        RD_LOG_FAIL(
            "plugin '%s' is not supported, expected API Level %d, got %d", id,
            RD_API_LEVEL, level);
        return false;
    }

    return true;
}

bool rd_i_validate_plugin_with_name(u32 level, const char* id, const char* name,
                                    const char* kind) {
    if(!rd_i_validate_plugin(level, id, kind)) return false;

    if(!name || !(*name)) {
        RD_LOG_FAIL("invalid name for plugin '%s'");
        return false;
    }

    return true;
}
