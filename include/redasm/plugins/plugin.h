#pragma once

#include <redasm/config.h>

#define RD_PLUGIN_HEADER                                                       \
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

typedef void (*RDModuleLoad)(void);
typedef void (*RDModuleUnload)(void);

// ABI CONTRACT:
// - 'api_version' MUST be the first field.
// - module loader reads it alone as a bare u32 before trusting anything else
//   about this struct's layout.
// - never reorder or insert a field before it even in a future major version,
//   the position is permanent.
typedef struct RDModuleDescriptor {
    u32 api_version;
    RDModuleLoad load;
    RDModuleUnload unload; // optional, may be NULL
} RDModuleDescriptor;

typedef struct RDModule {
    const char* path;
    u32 api_version;
} RDModule;

typedef struct RDModuleSlice {
    const RDModule** data;
    usize length;
} RDModuleSlice;

RD_API RDModuleSlice rd_get_all_modules(void);
RD_API RDPluginSlice rd_get_all_loader_plugins(void);
RD_API RDPluginSlice rd_get_all_processor_plugins(void);
RD_API RDPluginSlice rd_get_all_analyzer_plugins(void);
RD_API RDPluginSlice rd_get_all_command_plugins(void);
RD_API const RDLoaderPlugin* rd_loader_find(const char* id);
RD_API const RDProcessorPlugin* rd_processor_find(const char* id);
RD_API const RDAnalyzerPlugin* rd_analyzer_find(const char* id);
RD_API const RDCommandPlugin* rd_command_find(const char* id);

// Module entry point.
// Gives static analysis a visible external reference so 'rd_module' isn't
// flagged as a candidate for internal linkage.
// The actual definition lives in each plugin's own .c file via
// RD_MODULE_EXPORT.
#define RD_MODULE_EXPORT const RDModuleDescriptor rd_module

#ifdef __cplusplus
RD_API RD_MODULE_EXPORT;
#else
RD_API extern RD_MODULE_EXPORT;
#endif
