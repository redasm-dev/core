#pragma once

#include <redasm/common.h>
#include <redasm/io/reader.h>
#include <redasm/plugins/plugin.h>

typedef struct RDLoader RDLoader;

typedef struct RDLoaderRequest {
    RDReader* input;
    const char* filepath;
    const char* name;
    const char* ext;
} RDLoaderRequest;

typedef struct RDLoaderPlugin {
    RD_PLUGIN_HEADER;
    void* userdata;

    RDLoader* (*create)(const struct RDLoaderPlugin*);
    void (*destroy)(RDLoader*);

    bool (*parse)(RDLoader*, const RDLoaderRequest*);
    bool (*load)(RDLoader*, RDContext*);
    const char* (*get_processor)(RDLoader*, const RDContext*);
} RDLoaderPlugin;

RD_API bool rd_register_loader(const RDLoaderPlugin* l);
