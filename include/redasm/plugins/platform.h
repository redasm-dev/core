#pragma once

#include <redasm/context.h>
#include <redasm/plugins/plugin.h>

typedef struct RDPlatform RDPlatform;

typedef struct RDPlatformPlugin {
    RD_PLUGIN_HEADER

    usize instance_size;
    const char* name;

    RDPlatform* (*create)(const struct RDPlatformPlugin*);
    void (*destroy)(RDPlatform*);
    void (*setup)(RDContext*, RDPlatform*);
} RDPlatformPlugin;

RD_API bool rd_register_platform(const RDPlatformPlugin* p);
