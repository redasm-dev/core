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

typedef void (*RDModuleCreate)(void);
typedef void (*RDModuleDestroy)(void);
typedef const char* (*RDModuleVersion)(void);

typedef struct RDModuleFull {
    RDModule base;

    RDModuleHandle handle;
    RDModuleCreate create;
    RDModuleDestroy destroy;
} RDModuleFull;

RDModuleFull* rd_i_module_create(const char* filepath);
void rd_i_module_destroy(RDModuleFull* self);
