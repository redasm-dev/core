#include "option.h"
#include "core/context.h"
#include "plugins/loader/loader.h"
#include <redasm/support/logging.h>

static void _rd_register_current_group(RDLoaderOptionBuilder* self) {
    const char** it;

    vect_each(it, &self->groups) {
        if(*it == self->current_group) return; // interned, or both NULL
    }

    vect_push(&self->groups, self->current_group);
}

void rd_i_loader_option_init(RDLoaderOptionBuilder* self) {
    *self = (RDLoaderOptionBuilder){0};
    rd_i_strpool_init(&self->strings);
}

void rd_i_loader_option_deinit(RDLoaderOptionBuilder* self) {
    vect_destroy(&self->options);
    vect_destroy(&self->groups);
    rd_i_strpool_deinit(&self->strings);
    *self = (RDLoaderOptionBuilder){0};
}

void rd_loader_options_set_group(RDLoaderOptionBuilder* self,
                                 const char* group) {
    self->current_group = rd_i_strpool_intern(&self->strings, group);
    _rd_register_current_group(self);
}

void rd_loader_options_add_bool(RDLoaderOptionBuilder* self, const char* id,
                                const char* name, const char* desc, bool v) {
    if(!id) {
        RD_LOG_FAIL("loader option id is NULL");
        return;
    }

    if(!name) {
        RD_LOG_FAIL("loader option name is NULL for id '%s'", id);
        return;
    }

    const RDLoaderOption* opt;
    vect_each(opt, &self->options) {
        if(!strcmp(opt->id, id)) {
            RD_LOG_FAIL("duplicate loader option id '%s'", id);
            return;
        }
    }

    vect_push(&self->options,
              (RDLoaderOption){
                  .kind = RD_LOPT_BOOL,
                  .id = rd_i_strpool_intern(&self->strings, id),
                  .name = rd_i_strpool_intern(&self->strings, name),
                  .desc = rd_i_strpool_intern(&self->strings, desc),
                  .group = self->current_group,
                  .defvalue = v,
                  .value = v,
              });
}

bool rd_get_loader_option_bool(const RDContext* ctx, const char* id, bool* v) {
    if(!ctx || !id) return false;

    if(!ctx->testresult) {
        RD_LOG_FAIL("loader options are only readable during load()");
        return false;
    }

    const RDLoaderOption* opt;

    vect_each(opt, &ctx->testresult->loader_options.options) {
        if(strcmp(opt->id, id) != 0) continue;

        if(opt->kind != RD_LOPT_BOOL) {
            RD_LOG_FAIL("loader option '%s' is not a boolean", id);
            return false;
        }

        if(v) *v = opt->value;
        return true;
    }

    RD_LOG_FAIL("unknown loader option '%s'", id);
    return false;
}
