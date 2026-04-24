#pragma once

#include <redasm/context.h>
#include <redasm/function.h>
#include <redasm/graph/graph.h>
#include <redasm/graph/layout.h>
#include <redasm/mapping.h>
#include <redasm/plugins/analyzer.h>
#include <redasm/plugins/command.h>
#include <redasm/plugins/loader.h>
#include <redasm/plugins/processor/processor.h>
#include <redasm/plugins/processor/rdil.h>
#include <redasm/segment.h>
#include <redasm/support/utils.h>
#include <redasm/surface/graph.h>
#include <redasm/surface/renderer.h>
#include <redasm/surface/surface.h>
#include <redasm/theme.h>
#include <redasm/types/def.h>
#include <redasm/types/type.h>

typedef struct RDContextSlice {
    RDContext* const* data;
    usize length;
} RDContextSlice;

RD_API void rd_init(void);
RD_API void rd_deinit(void);
RD_API RDContextSlice rd_test(const char* filepath);
RD_API bool rd_module_load(const char* filepath);
RD_API bool rd_accept(const RDContext* ctx, const RDProcessorPlugin* p);
RD_API void rd_reject(void);
