#pragma once

#include <redasm/config.h>

#define RD_PLUGIN_HEADER                                                       \
    u32 level;                                                                 \
    u32 flags;                                                                 \
    void* userdata;                                                            \
    const char* id

#define RD_PF_LAST (1u << 31)

typedef struct RDLoaderPlugin RDLoaderPlugin;
typedef struct RDProcessorPlugin RDProcessorPlugin;
typedef struct RDAnalyzerPlugin RDAnalyzerPlugin;
typedef struct RDCommandPlugin RDCommandPlugin;

typedef struct RDPlugin {
    union {
        const RDLoaderPlugin* loader;
        const RDProcessorPlugin* processor;
        const RDAnalyzerPlugin* analyzer;
        const RDCommandPlugin* command;
    };
} RDPlugin;

typedef struct RDPluginSlice {
    const RDPlugin** data;
    usize length;
} RDPluginSlice;

typedef struct RDModule {
    const char* path;
    const char* version;
} RDModule;

typedef struct RDModuleSlice {
    const RDModule** data;
    usize length;
} RDModuleSlice;

RD_API RDModuleSlice rd_get_all_modules(void);
RD_API RDPluginSlice rd_get_all_loader_plugins(void);
RD_API RDPluginSlice rd_get_all_processor_plugins(void);
RD_API RDPluginSlice rd_get_all_analyzers_plugins(void);
RD_API RDPluginSlice rd_get_all_command_plugins(void);
RD_API const RDLoaderPlugin* rd_loader_find(const char* id);
RD_API const RDProcessorPlugin* rd_processor_find(const char* id);
RD_API const RDAnalyzerPlugin* rd_analyzer_find(const char* id);
RD_API const RDCommandPlugin* rd_command_find(const char* id);

// Plugin entry points
RD_API void rd_plugin_create(void);
RD_API void rd_plugin_destroy(void);
RD_API const char* rd_plugin_version(void);
