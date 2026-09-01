#include "common.h"
#include <redasm/support/logging.h>
#include <redasm/version.h>

bool rd_i_validate_plugin(const char* id, const char* kind) {
    if(!id || !(*id)) {
        RD_LOG_FAIL("invalid %s-plugin id", kind);
        return false;
    }

    return true;
}

bool rd_i_validate_plugin_with_name(const char* id, const char* name,
                                    const char* kind) {
    if(!rd_i_validate_plugin(id, kind)) return false;

    if(!name || !(*name)) {
        RD_LOG_FAIL("invalid name for plugin '%s'", id);
        return false;
    }

    return true;
}
