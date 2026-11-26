#pragma once

#include <redasm/common.h>
#include <redasm/plugins/common.h>
#include <redasm/plugins/loader/option.h>
#include <redasm/plugins/plugin.h>

typedef struct RDLoader RDLoader;

typedef enum {
    RD_LF_NONE = 0,
    RD_LF_MANUAL = (1 << 0),
} RDLoaderFlags;

typedef struct RDLoaderPlugin {
    RD_PLUGIN_HEADER

    usize instance_size;

    RDLoader* (*create)(const struct RDLoaderPlugin*);
    void (*destroy)(RDLoader*);

    bool (*parse)(RDLoader*, const RDParseRequest*);
    bool (*load)(RDLoader*, RDContext*);
    const char* (*get_name)(const RDLoader*);
    const char* (*get_processor)(const RDLoader*);
    const char* (*get_platform)(const RDLoader*);
    void (*get_options)(RDLoader* self, RDLoaderOptionBuilder* b);
} RDLoaderPlugin;

RD_API bool rd_register_loader(const RDLoaderPlugin* l);
