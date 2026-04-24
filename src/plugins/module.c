#include "plugins/module.h"
#include "support/logging.h"
#include <assert.h>
#include <redasm/support/utils.h>
#include <stdlib.h>
#include <string.h>

#define RD_PLUGIN_CREATE "rd_plugin_create"
#define RD_PLUGIN_DESTROY "rd_plugin_destroy"

#if defined(_WIN32)
typedef FARPROC RDModuleProc;
#else
typedef void* RDModuleProc;
#endif

static void _rd_module_sym(const RDModule* self, const char* name, void* sym) {
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
                  &buffer, 0, NULL);

    LOG_FAIL("%s", buffer);
    LocalFree(buffer);
#else
    LOG_FAIL("%s", dlerror());
#endif
}

RDModule* rd_i_module_create(const char* filepath) {
    if(!filepath) return NULL;

    LOG_INFO("Loading module '%s'", filepath);
    RDModule* self = calloc(1, sizeof(*self));

#if defined(_WIN32)
    self->handle = LoadLibraryA(filepath);
#else
    self->handle = dlopen(filepath, RTLD_LAZY);
#endif

    self->path = rd_strdup(filepath);

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
    return self;

fail:
    rd_i_module_destroy(self);
    return NULL;
}

void rd_i_module_destroy(RDModule* self) {
    LOG_INFO("Unloading module '%s'", self->path);

    if(self->handle) {
#if defined(_WIN32)
        FreeLibrary(self->handle);
#else
        dlclose(self->handle);
#endif
    }

    rd_free(self->path);
    free(self);
}
