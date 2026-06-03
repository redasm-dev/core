#pragma once

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

typedef struct RDModule {
    char* path;

    RDModuleHandle handle;
    RDModuleCreate create;
    RDModuleDestroy destroy;

    struct RDModule* prev;
    struct RDModule* next;
} RDModule;

RDModule* rd_i_module_create(const char* filepath);
void rd_i_module_destroy(RDModule* self);
