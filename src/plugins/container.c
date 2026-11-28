#include "container.h"
#include "core/state.h"
#include "plugins/common.h"
#include <redasm/allocator.h>
#include <redasm/support/logging.h>

RDContainer* rd_i_container_create(const RDContainerPlugin* plugin) {
    if(!plugin) return NULL;

    if(plugin->create) return plugin->create(plugin);
    if(plugin->instance_size) return rd_alloc0(1, plugin->instance_size);
    return NULL;
}

void rd_i_container_destroy(const RDContainerPlugin* plugin, RDContainer* ldr) {
    if(!plugin) return;

    if(plugin->destroy) {
        plugin->destroy(ldr);
        return;
    }

    if(!plugin->create && plugin->instance_size) rd_free(ldr);
}

bool rd_register_container(const RDContainerPlugin* c) {
    if(!rd_i_validate_plugin(c->id, "container")) return false;

    if(!c->get_name) {
        RD_LOG_FAIL("container '%s' requires a name", c->id);
        return false;
    }

    if(!c->parse) {
        RD_LOG_FAIL("container '%s' requires a parse function", c->id);
        return false;
    }

    // if(!c->extract) {
    //     RD_LOG_FAIL("container '%s' requires an extract function", c->id);
    //     return false;
    // }

    if(c->create && c->instance_size) {
        RD_LOG_WARN("container '%s': 'instance_size' is ignored because "
                    "'create' is set",
                    c->id);
    }

    if(rd_container_find(c->id)) {
        RD_LOG_WARN("container '%s' already registered", c->id);
        return false;
    }

    RD_LOG_DEBUG("registering container '%s'", c->id);
    RDPlugin* plugin = rd_alloc(sizeof(*plugin));
    plugin->container = c;
    vect_push(&rd_i_state.containers, plugin);
    return true;
}
