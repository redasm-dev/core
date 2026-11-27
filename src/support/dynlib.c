#include "dynlib.h"
#include <assert.h>
#include <redasm/support/logging.h>
#include <string.h>

// clang-format off
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif
// clang-format on

static_assert(sizeof(void (*)(void)) == sizeof(void*),
              "function and data pointers must have the same size");

RDDynLib* rd_i_dynlib_open(const char* filepath) {
    if(!filepath) return NULL;

#if defined(_WIN32)
    return (RDDynLib*)LoadLibraryA(filepath);
#else
    return (RDDynLib*)dlopen(filepath, RTLD_LAZY);
#endif
}

void rd_i_dynlib_close(RDDynLib* self) {
    if(!self) return;

#if defined(_WIN32)
    FreeLibrary((HMODULE)self);
#else
    dlclose(self);
#endif
}

const void* rd_i_dynlib_sym(const RDDynLib* self, const char* name) {
    if(!self || !name) return NULL;

#if defined(_WIN32)
    // GetProcAddress returns FARPROC even for data symbols: copy the
    // same-sized bit pattern instead of relying on an implicit conversion
    FARPROC proc = GetProcAddress((HMODULE)self, name);
    const void* out;
    memcpy(&out, &proc, sizeof(out));
    return out;
#else
    return dlsym((void*)self, name); // POSIX guarantees data-pointer-safe
#endif
}

bool rd_i_dynlib_func(const RDDynLib* self, const char* name, void* fnptr) {
    if(!self || !name || !fnptr) return false;

#if defined(_WIN32)
    FARPROC proc = GetProcAddress((HMODULE)self, name);
    memcpy(fnptr, &proc, sizeof(proc));
    return proc != NULL;
#else
    void* sym = dlsym((void*)self, name);
    memcpy(fnptr, (void*)&sym, sizeof(sym));
    return sym != NULL;
#endif
}

void rd_i_dynlib_log_error(void) {
#if defined(_WIN32)
    DWORD error = GetLastError();
    LPSTR buffer = NULL;

    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                   NULL, error, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                   (LPSTR)&buffer, 0, NULL);

    if(buffer) {
        RD_LOG_FAIL("%s", buffer);
        LocalFree(buffer);
    }
    else
        RD_LOG_FAIL("error %lu", (unsigned long)error);
#else
    const char* err = dlerror();
    if(err) RD_LOG_FAIL("%s", err);
#endif
}
