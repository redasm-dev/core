#pragma once

#include "support/dynlib.h"
#include <redasm/plugins/plugin.h>

typedef struct RDModuleFull {
    RDModule base;
    RDDynLib* handle;
    const RDModuleDescriptor* descr;
} RDModuleFull;

RDModuleFull* rd_i_module_create(const char* filepath);
void rd_i_module_destroy(RDModuleFull* self);
