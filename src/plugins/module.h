#pragma once

#include <redasm/plugins/plugin.h>

// clang-format off
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winbase.h>
#else
#include <dlfcn.h>
#endif
// clang-format on

#if defined(_WIN32)
typedef HMODULE RDModuleHandle;
#else
typedef void* RDModuleHandle;
#endif

typedef struct RDModuleFull {
    RDModule base;
    RDModuleHandle handle;
    const RDModuleDescriptor* descr;
} RDModuleFull;

RDModuleFull* rd_i_module_create(const char* filepath);
void rd_i_module_destroy(RDModuleFull* self);
