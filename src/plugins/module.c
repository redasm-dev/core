#include "plugins/module.h"
#include "core/state.h"
#include <redasm/allocator.h>
#include <redasm/support/logging.h>
#include <redasm/support/utils.h>

#define RD_MODULE_SYMBOL "rd_module"

RDModuleFull* rd_i_module_create(const char* filepath) {
    if(!filepath) return NULL;

    RD_LOG_INFO("Loading module '%s'", filepath);
    RDModuleFull* self = rd_alloc0(1, sizeof(*self));
    self->handle = rd_i_dynlib_open(filepath);
    self->base.path = rd_strdup(filepath);

    if(!self->handle) {
        RD_LOG_FAIL("failed to load '%s'", filepath);
        rd_i_dynlib_log_error();
        goto fail;
    }

    self->descr = (const RDModuleDescriptor*)rd_i_dynlib_sym(self->handle,
                                                             RD_MODULE_SYMBOL);

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

    return self;

fail:
    rd_i_module_destroy(self);
    return NULL;
}

void rd_i_module_destroy(RDModuleFull* self) {
    RD_LOG_INFO("Unloading module '%s'", self->base.path);
    rd_i_dynlib_close(self->handle);
    rd_free((char*)self->base.path);
    rd_free(self);
}
