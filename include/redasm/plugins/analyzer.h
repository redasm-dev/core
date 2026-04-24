#pragma once

#include <redasm/common.h>
#include <redasm/plugins/plugin.h>

typedef enum RDAnalyzerFlags {
    RD_AF_SELECTED = (1 << 0),
    RD_AF_RUNONCE = (1 << 1),
    RD_AF_EXPERIMENTAL = (1 << 2),
} RDAnalyzerFlags;

typedef struct RDAnalyzerPlugin {
    RD_PLUGIN_HEADER;

    u32 order;
    bool (*is_enabled)(const struct RDAnalyzerPlugin*);
    void (*execute)(RDContext*);
} RDAnalyzerPlugin;

typedef struct RDAnalyzerItem RDAnalyzerItem;

typedef struct RDAnalyzerItemSlice {
    RDAnalyzerItem** data;
    usize length;
} RDAnalyzerItemSlice;

RD_API bool rd_register_analyzer(const RDAnalyzerPlugin* a);

// clang-format off
RD_API const RDAnalyzerPlugin* rd_analyzeritem_get_plugin(const RDAnalyzerItem* self);
RD_API bool rd_analyzeritem_is_selected(const RDAnalyzerItem* self);
RD_API void rd_analyzeritem_select(RDAnalyzerItem* self, bool select);
// clang-format on
