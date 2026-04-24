#pragma once

#if defined(_WIN32)
#include <winbase.h>
#include <windows.h>
#else
#include <dlfcn.h>
#endif

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
