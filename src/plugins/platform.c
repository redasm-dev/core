#include "platform.h"
#include "core/state.h"
#include "plugins/common.h"
#include <redasm/support/logging.h>

RDPlatform* rd_i_platform_create(const RDPlatformPlugin* plugin) {
    if(!plugin) return NULL;

    if(plugin->create) return plugin->create(plugin);
    if(plugin->instance_size) return rd_alloc0(1, plugin->instance_size);
    return NULL;
}

void rd_i_platform_destroy(const RDPlatformPlugin* plugin, RDPlatform* p) {
    if(!plugin) return;

    if(plugin->destroy) {
        plugin->destroy(p);
        return;
    }

    if(!plugin->create && plugin->instance_size) rd_free(p);
}

bool rd_register_platform(const RDPlatformPlugin* p) {
    if(!rd_i_validate_plugin_with_name(p->id, p->name, "platform"))
        return false;

    if(!p->setup) {
        RD_LOG_FAIL("platform '%s' requires a setup", p->id);
        return false;
    }

    if(rd_platform_find(p->id)) {
        RD_LOG_WARN("platform '%s' already registered", p->id);
        return false;
    }

    RD_LOG_DEBUG("registering platform '%s' [%s]", p->id, p->name);
    RDPlugin* plugin = rd_alloc(sizeof(*plugin));
    plugin->platform = p;
    vect_push(&rd_i_state.platforms, plugin);
    return true;
}
