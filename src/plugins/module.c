#include "plugins/module.h"
#include "core/state.h"
#include "support/logging.h"
#include "support/stringpool.h"
#include <assert.h>
#include <redasm/allocator.h>
#include <redasm/support/utils.h>
#include <string.h>

#define RD_MODULE_VERSION_NONE "unknown"

#define RD_PLUGIN_CREATE "rd_plugin_create"
#define RD_PLUGIN_DESTROY "rd_plugin_destroy"
#define RD_PLUGIN_VERSION "rd_plugin_version"

#if defined(_WIN32)
typedef FARPROC RDModuleProc;
#else
typedef void* RDModuleProc;
#endif

static void _rd_module_sym(const RDModuleFull* self, const char* name,
                           void* sym) {
#if defined(_WIN32)
    RDModuleProc proc = GetProcAddress(self->handle, name);
#else
    RDModuleProc proc = dlsym(self->handle, name);
#endif

    // data-pointer vs function-pointer size can be different
    // in some platforms, check the size.
    static_assert(sizeof(proc) == sizeof(RDModuleCreate),
                  "function pointer size mismatch");
    memcpy(sym, (char*)&proc, sizeof(proc));
}

static void _rd_module_errmsg(void) {
#if defined(_WIN32)
    DWORD error = GetLastError();
    LPTSTR buffer = NULL;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                  NULL, error, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                  (LPTSTR)(LPVOID)&buffer, 0, NULL);

    LOG_FAIL("%s", buffer);
    LocalFree(buffer);
#else
    LOG_FAIL("%s", dlerror());
#endif
}

RDModuleFull* rd_i_module_create(const char* filepath) {
    if(!filepath) return NULL;

    LOG_INFO("Loading module '%s'", filepath);
    RDModuleFull* self = rd_alloc0(1, sizeof(*self));

#if defined(_WIN32)
    self->handle = LoadLibraryA(filepath);
#else
    self->handle = dlopen(filepath, RTLD_LAZY);
#endif

    self->base.path = rd_strdup(filepath);

    if(!self->handle) {
        LOG_FAIL("failed to load '%s'", filepath);
        _rd_module_errmsg();
        goto fail;
    }

    _rd_module_sym(self, RD_PLUGIN_CREATE, (void*)&self->create);

    if(!self->create) {
        LOG_FAIL("'%s' is not a valid plugin", filepath);
        goto fail;
    }

    _rd_module_sym(self, RD_PLUGIN_DESTROY, (void*)&self->destroy);

    RDModuleVersion module_ver;
    _rd_module_sym(self, RD_PLUGIN_VERSION, (void*)&module_ver);

    if(module_ver)
        self->base.version =
            rd_i_strpool_intern(&rd_i_state.strings, module_ver());

    if(!self->base.version) self->base.version = RD_MODULE_VERSION_NONE;

    return self;

fail:
    rd_i_module_destroy(self);
    return NULL;
}

void rd_i_module_destroy(RDModuleFull* self) {
    LOG_INFO("Unloading module '%s'", self->base.path);

    if(self->handle) {
#if defined(_WIN32)
        FreeLibrary(self->handle);
#else
        dlclose(self->handle);
#endif
    }

    rd_free((char*)self->base.path);
    rd_free(self);
}
