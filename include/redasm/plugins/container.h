#pragma once

#include <redasm/common.h>
#include <redasm/plugins/common.h>
#include <redasm/plugins/plugin.h>

typedef struct RDContainer RDContainer;

typedef struct RDContainerPlugin {
    RD_PLUGIN_HEADER

    usize instance_size;

    RDContainer* (*create)(const struct RDContainerPlugin*);
    void (*destroy)(RDContainer*);

    bool (*parse)(RDContainer*, const RDParseRequest*);
    const char* (*get_name)(const RDContainer*);

} RDContainerPlugin;

RD_API bool rd_register_container(const RDContainerPlugin* c);
