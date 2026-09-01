#include "plugins/module.h"
#include "core/state.h"
#include <assert.h>
#include <redasm/allocator.h>
#include <redasm/support/logging.h>
#include <redasm/support/utils.h>
#include <string.h>

#define RD_MODULE_SYMBOL "rd_module"

static const void* _rd_module_sym(const RDModuleFull* self, const char* name) {
#if defined(_WIN32)
    // GetProcAddress always returns FARPROC even for data symbols on Windows.
    // Reinterpret through the same-sized bit pattern rather than assume
    // implicit convertibility.
    FARPROC proc = GetProcAddress(self->handle, name);
    static_assert(sizeof(proc) == sizeof(void*), "pointer size mismatch");
    const void* out;
    memcpy(&out, &proc, sizeof(proc));
    return out;
#else
    return dlsym(self->handle, name); // POSIX guarantees data-pointer-safe
#endif
}

static void _rd_module_errmsg(void) {
#if defined(_WIN32)
    DWORD error = GetLastError();
    LPTSTR buffer = NULL;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                  NULL, error, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                  (LPTSTR)(LPVOID)&buffer, 0, NULL);

    RD_LOG_FAIL("%s", buffer);
    LocalFree(buffer);
#else
    RD_LOG_FAIL("%s", dlerror());
#endif
}

RDModuleFull* rd_i_module_create(const char* filepath) {
    if(!filepath) return NULL;

    RD_LOG_INFO("Loading module '%s'", filepath);
    RDModuleFull* self = rd_alloc0(1, sizeof(*self));

#if defined(_WIN32)
    self->handle = LoadLibraryA(filepath);
#else
    self->handle = dlopen(filepath, RTLD_LAZY);
#endif

    self->base.path = rd_strdup(filepath);

    if(!self->handle) {
        RD_LOG_FAIL("failed to load '%s'", filepath);
        _rd_module_errmsg();
        goto fail;
    }

    self->descr =
        (const RDModuleDescriptor*)_rd_module_sym(self, RD_MODULE_SYMBOL);

    if(!self->descr) {
        RD_LOG_FAIL("'%s' is not a valid plugin", filepath);
        goto fail;
    }

    if(self->descr->api_version != RD_API_VERSION) {
        RD_LOG_FAIL("'%s': API mismatch, expected v%u.%u, got v%u.%u", filepath,
                    RD_VERSION_MAJOR, RD_VERSION_MINOR,
                    RD_API_VERSION_MAJOR(self->descr->api_version),
                    RD_API_VERSION_MINOR(self->descr->api_version));
        goto fail;
    }

    // copy API Version to public interface
    self->base.api_version = self->descr->api_version;
    return self;

fail:
    rd_i_module_destroy(self);
    return NULL;
}

void rd_i_module_destroy(RDModuleFull* self) {
    RD_LOG_INFO("Unloading module '%s'", self->base.path);

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
