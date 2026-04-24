#pragma once

#include <redasm/config.h>

#define RD_PLUGIN_HEADER                                                       \
    u32 level;                                                                 \
    u32 flags;                                                                 \
    const char* id;                                                            \
    const char* name

#define PF_LAST (1u << 31)

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
