#include "core/context.h"
#include "core/state.h"
#include "io/buffer.h"
#include "io/reader.h"
#include "plugins/builtin/builtin.h"
#include "support/containers.h"
#include "support/logging.h"
#include "support/utils.h"
#include <assert.h>
#include <redasm/plugins/analyzer.h>
#include <redasm/plugins/command.h>
#include <redasm/plugins/loader.h>
#include <redasm/plugins/processor/processor.h>
#include <stdlib.h>

static bool _rd_part_loaders(const RDContext** ctx) {
    return !((*ctx)->loaderplugin->flags & PF_LAST);
}

static int _rd_analyzers_cmp(const void* arg1, const void* arg2) {
    const RDAnalyzerPlugin* a1 = *(const RDAnalyzerPlugin**)arg1;
    const RDAnalyzerPlugin* a2 = *(const RDAnalyzerPlugin**)arg2;
    if(a1->order < a2->order) return -1;
    if(a1->order > a2->order) return 1;
    return 0;
}

static bool _rd_validate_plugin(int level, const char* id, const char* name) {
    if(!id || !(*id)) {
        LOG_FAIL("invalid plugin id");
        return false;
    }

    if(level != RD_API_LEVEL) {
        LOG_FAIL("plugin '%s' is not supported, expected API Level %d, got %d",
                 id, RD_API_LEVEL, level);
        return false;
    }

    if(!name || !(*name)) {
        LOG_FAIL("invalid name for plugin '%s'");
        return false;
    }

    return true;
}

void rd_init(void) { rd_i_state_init(); }
void rd_deinit(void) { rd_i_state_deinit(); }

RDContextSlice rd_test(const char* filepath) {
    vect_destroy(&rd_i_state.tests);

    RDByteBuffer* input = rd_i_readfile(filepath);

    if(!rd_i_buffer_is_empty((RDBuffer*)input)) {
        RDPlugin** it;
        vect_each(it, &rd_i_state.loaders) {
            const RDLoaderPlugin* p = (*it)->loader;
            LOG_TRACE("testing '%s' ...", p->name);

            RDLoader* ldr = p->create ? p->create(p) : NULL;

            RDLoaderRequest req = {
                .filepath = filepath,
                .input = rd_i_reader_create((RDBuffer*)input),
                .name = rd_i_get_file_name(filepath),
                .ext = rd_i_get_file_ext(filepath),
            };

            if(p->parse(ldr, &req)) {
                RDContext* ctx = rd_i_context_create(p, ldr, filepath, input);

                if(p->get_processor) {
                    const char* procid = p->get_processor(ldr, ctx);
                    if(procid) ctx->processorplugin = rd_processor_find(procid);
                }

                if(!ctx->processorplugin) {
                    ctx->processorplugin =
                        rd_processor_find(RD_NULL_PROCESSOR_ID);
                }

                assert(ctx->processorplugin && "invalid processor plugin");
                vect_push(&rd_i_state.tests, ctx);
            }
            else if(p->destroy)
                p->destroy(ldr);

            rd_i_reader_destroy(req.input);
        }

        // Sort results by flags
        vect_stable_part(&rd_i_state.tests, _rd_part_loaders);
        return vect_to_slice(RDContextSlice, &rd_i_state.tests);
    }

    return (RDContextSlice){0};
}

bool rd_accept(const RDContext* self, const RDProcessorPlugin* p) {
    if(!self) return false;

    bool accepted = false;

    RDContext** it;
    vect_each(it, &rd_i_state.tests) {
        RDContext* ctx = *it;

        if(ctx != self) {
            ctx->input = NULL; // disown input
            rd_destroy(ctx);
            continue;
        }

        if(p) ctx->processorplugin = p;
        assert(ctx->processorplugin && "processor plugin not set");

        if(ctx->processorplugin->create)
            ctx->processor = ctx->processorplugin->create(ctx->processorplugin);

        if(ctx->loaderplugin->load &&
           ctx->loaderplugin->load(ctx->loader, ctx)) {
            RDPlugin** it;
            vect_each(it, &rd_i_state.analyzers) {
                const RDAnalyzerPlugin* p = (*it)->analyzer;

                // Assume true if 'is_enabled' is not implemented
                if(!p->is_enabled || p->is_enabled(p)) {
                    RDAnalyzerItem* ai = malloc(sizeof(*ai));

                    *ai = (RDAnalyzerItem){
                        .plugin = p,
                        .is_selected = p->flags & RD_AF_SELECTED,
                    };

                    vect_push(&ctx->analyzerplugins, ai);
                }
            }

            accepted = true;
        }
    }

    vect_clear(&rd_i_state.tests);
    return accepted;
}

void rd_reject(void) {
    RDContext** ctx;
    bool needsdisown = false;

    vect_each(ctx, &rd_i_state.tests) {
        if(needsdisown) (*ctx)->input = NULL;
        rd_destroy(*ctx);
        needsdisown = true;
    }

    vect_clear(&rd_i_state.tests);
}

bool rd_register_loader(const RDLoaderPlugin* l) {
    if(!_rd_validate_plugin(l->level, l->id, l->name)) return false;

    if(!l->parse) {
        LOG_FAIL("loader '%s' does not have a parser", l->id);
        return false;
    }

    if(rd_loader_find(l->id)) {
        LOG_WARN("loader '%s' already registered", l->id);
        return false;
    }

    LOG_TRACE("registering loader '%s' [%s]", l->id, l->name);
    RDPlugin* plugin = malloc(sizeof(*plugin));
    plugin->loader = l;
    vect_push(&rd_i_state.loaders, plugin);
    return true;
}

bool rd_register_processor(const RDProcessorPlugin* p) {
    if(!_rd_validate_plugin(p->level, p->id, p->name)) return false;

    if(!p->decode) {
        LOG_FAIL("processor '%s' does not have a decoder", p->id);
        return false;
    }

    if(!p->emulate) {
        LOG_FAIL("processor '%s' does not have an emulator", p->id);
        return false;
    }

    if(rd_processor_find(p->id)) {
        LOG_WARN("processor '%s' already registered", p->id);
        return false;
    }

    LOG_TRACE("registering processor '%s' [%s]", p->id, p->name);
    RDPlugin* plugin = malloc(sizeof(*plugin));
    plugin->processor = p;
    vect_push(&rd_i_state.processors, plugin);
    return true;
}

bool rd_register_analyzer(const RDAnalyzerPlugin* a) {
    if(!_rd_validate_plugin(a->level, a->id, a->name)) return false;

    if(!a->execute) {
        LOG_FAIL("analyzer '%s' does not have an execution", a->id);
        return false;
    }

    if(rd_analyzer_find(a->id)) {
        LOG_WARN("analyzer '%s' already registered", a->id);
        return false;
    }

    LOG_TRACE("registering analyzer '%s' [%s]", a->id, a->name);
    RDPlugin* plugin = malloc(sizeof(*plugin));
    plugin->analyzer = a;
    vect_push(&rd_i_state.analyzers, plugin);
    vect_sort(&rd_i_state.analyzers, _rd_analyzers_cmp);
    return true;
}

bool rd_register_command(const RDCommandPlugin* c) {
    if(!_rd_validate_plugin(c->level, c->id, c->name)) return false;

    if(!c->execute) {
        LOG_FAIL("command '%s' does not have an execution", c->id);
        return false;
    }

    if(rd_command_find(c->id)) {
        LOG_WARN("command '%s' already registered", c->id);
        return false;
    }

    LOG_TRACE("registering command '%s' [%s]", c->id, c->name);
    RDPlugin* plugin = malloc(sizeof(*plugin));
    plugin->command = c;
    vect_push(&rd_i_state.commands, plugin);
    return true;
}
