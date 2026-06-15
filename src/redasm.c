#include "core/context.h"
#include "core/state.h"
#include "io/buffer.h"
#include "io/reader.h"
#include "plugins/builtin/builtin.h"
#include "plugins/processor/processor.h"
#include "support/containers.h"
#include "support/logging.h"
#include "support/utils.h"
#include "surface/renderer.h"
#include <assert.h>
#include <redasm/plugins/analyzer.h>
#include <redasm/plugins/command.h>
#include <redasm/plugins/loader.h>
#include <redasm/plugins/processor/processor.h>

static bool _rd_part_loaders(const RDTestResult** tr) {
    return !((*tr)->loaderplugin->flags & RD_PF_LAST);
}

static char* _rd_derive_dbpath(const RDTestResult* tr,
                               const RDAcceptParams* params) {
    if(!strcmp(tr->filepath, ":memory:")) return rd_strdup(tr->filepath);

    char* stem = rd_i_get_file_stem(tr->filepath);

    if(!stem) {
        LOG_FAIL("stem resolution failed for '%s'", tr->filepath);
        return NULL;
    }

    RDCharVect db_path_buf = {0};
    char* dbpath = NULL;

    if(!params->db_path) {
        char* path = rd_i_get_file_path(tr->filepath);

        if(!path) {
            LOG_FAIL("path resolution failed for '%s'", tr->filepath);
            goto cleanup;
        }

        dbpath = rd_strdup(
            rd_i_format(&db_path_buf, "%s%c%s.db", path, RD_PATH_SEP, stem));

        rd_free(path);
    }
    else {
        dbpath = rd_strdup(rd_i_format(&db_path_buf, "%s%c%s.db",
                                       params->db_path, RD_PATH_SEP, stem));
    }

cleanup:
    vect_destroy(&db_path_buf);
    rd_free(stem);
    return dbpath;
}

static int _rd_analyzers_cmp(const void* arg1, const void* arg2) {
    const RDAnalyzerPlugin* a1 = *(const RDAnalyzerPlugin**)arg1;
    const RDAnalyzerPlugin* a2 = *(const RDAnalyzerPlugin**)arg2;
    if(a1->order < a2->order) return -1;
    if(a1->order > a2->order) return 1;
    return 0;
}

static bool _rd_validate_plugin(u32 level, const char* id, const char* kind) {
    if(!id || !(*id)) {
        LOG_FAIL("invalid %s-plugin id", kind);
        return false;
    }

    if(level != RD_API_LEVEL) {
        LOG_FAIL("plugin '%s' is not supported, expected API Level %d, got %d",
                 id, RD_API_LEVEL, level);
        return false;
    }

    return true;
}

static bool _rd_validate_plugin_with_name(u32 level, const char* id,
                                          const char* name, const char* kind) {
    if(!_rd_validate_plugin(level, id, kind)) return false;

    if(!name || !(*name)) {
        LOG_FAIL("invalid name for plugin '%s'");
        return false;
    }

    return true;
}

static RDTestResultSlice rd_i_test(RDByteBuffer* inputbuf,
                                   const char* filepath) {
    if(!inputbuf) return (RDTestResultSlice){0};

    assert(filepath);
    vect_destroy(&rd_i_state.tests);

    if(!rd_i_buffer_is_empty((RDBuffer*)inputbuf)) {
        RDPlugin** it;
        vect_each(it, &rd_i_state.loaders) {
            const RDLoaderPlugin* plugin = (*it)->loader;
            LOG_DEBUG("testing '%s' ...", plugin->id);

            RDLoader* ldr = plugin->create ? plugin->create(plugin) : NULL;

            RDLoaderRequest req = {
                .filepath = filepath,
                .input = rd_i_reader_create((RDBuffer*)inputbuf),
                .name = rd_i_get_file_name(filepath),
                .ext = rd_i_get_file_ext(filepath),
            };

            if(plugin->parse(ldr, &req)) {
                RDTestResult* tr =
                    rd_i_testresult_create(plugin, ldr, inputbuf, filepath);

                if(plugin->get_processor) {
                    const char* procid = plugin->get_processor(ldr);
                    if(procid) tr->processorplugin = rd_processor_find(procid);
                }

                if(!tr->processorplugin) {
                    tr->processorplugin =
                        rd_processor_find(RD_NULL_PROCESSOR_ID);
                }

                assert(plugin->get_name);
                assert(tr->processorplugin);

                tr->loader_name = rd_strdup(plugin->get_name(ldr));
                if(!tr->loader_name) tr->loader_name = rd_strdup(plugin->id);
                assert(tr->loader_name);

                vect_push(&rd_i_state.tests, tr);
            }
            else if(plugin->destroy)
                plugin->destroy(ldr);

            rd_i_reader_destroy(req.input);
        }

        // Sort results by flags
        vect_stable_part(&rd_i_state.tests, _rd_part_loaders);

        return (RDTestResultSlice){
            .data = (const RDTestResult**)rd_i_state.tests.data,
            .length = rd_i_state.tests.length,
        };
    }

    return (RDTestResultSlice){0};
}

void rd_init(const RDInitParams* params) { rd_i_state_init(params); }
void rd_deinit(void) { rd_i_state_deinit(); }

RDTestResultSlice rd_test_data(const char* data, usize n) {
    if(!data || !n) return (RDTestResultSlice){0};

    RDByteBuffer* input = rd_i_fromdata(data, n);
    return rd_i_test(input, ":memory:");
}

RDTestResultSlice rd_test(const char* filepath) {
    if(!filepath) return (RDTestResultSlice){0};

    RDByteBuffer* input = rd_i_readfile(filepath);
    return rd_i_test(input, filepath);
}

RDAcceptResult rd_accept(const RDTestResult* tr, const RDAcceptParams* params) {
    if(!tr || !params) return (RDAcceptResult){.status = RD_ACCEPT_FAIL};

    RDAcceptResult res = {0};
    RDTestResult** it;
    char* dbpath = NULL;

    const RDProcessorPlugin* pplugin =
        params->processorplugin ? params->processorplugin : tr->processorplugin;

    if(!pplugin) {
        LOG_FAIL("processor plugin not set for loader '%s'",
                 tr->loaderplugin->id);
        goto cleanup;
    }

    if(params->mode == RD_AM_PROJECT) { // TODO: implement .rdx loading
        LOG_FAIL("project loading not yet implemented");
        goto cleanup;
    }

    dbpath = _rd_derive_dbpath(tr, params);
    if(!dbpath) goto cleanup;

    if(!rd_i_path_is_writable(dbpath)) {
        rd_free(dbpath);
        return (RDAcceptResult){.status = RD_ACCEPT_FAIL_WRITE};
    }

    if(rd_i_file_exists(dbpath)) {
        if(params->mode == RD_AM_NEW ||
           (params->mode == RD_AM_DATABASE && !rd_i_db_is_valid(dbpath)))
            remove(dbpath);
    }

    res.context = rd_i_context_create(tr->loaderplugin, tr->loader, pplugin,
                                      tr->input_buffer, tr->filepath, dbpath);
    if(!res.context) goto cleanup;

    ((RDTestResult*)tr)->owns_loader = false; // owned by context

    res.context->min_string =
        params->min_string ? params->min_string : RD_MIN_STRING_LENGTH;
    res.context->addressing = params->addressing;

    rd_reader_seek(res.context->input_reader, 0);

    if(res.context->loaderplugin->load(res.context->loader, res.context)) {
        LOG_INFO("selected loader '%s' and processor '%s'",
                 res.context->loaderplugin->id,
                 res.context->processorplugin->id);
    }
    else {
        rd_destroy(res.context);
        res.context = NULL;
        goto cleanup;
    }

    rd_reader_seek(res.context->input_reader, 0);

cleanup:
    vect_each(it, &rd_i_state.tests) rd_i_testresult_destroy(*it);
    vect_clear(&rd_i_state.tests);
    rd_free(dbpath);
    res.status = res.context ? RD_ACCEPT_OK : RD_ACCEPT_FAIL;
    return res;
}

void rd_reject(void) {
    RDTestResult** tr;
    RDByteBuffer* inputbuf = NULL;

    vect_each(tr, &rd_i_state.tests) {
        inputbuf = (*tr)->input_buffer;
        rd_i_testresult_destroy(*tr);
    }

    assert(inputbuf);
    rd_i_buffer_destroy((RDBuffer*)inputbuf);
    vect_clear(&rd_i_state.tests);
}

bool rd_register_loader(const RDLoaderPlugin* l) {
    if(!_rd_validate_plugin(l->level, l->id, "loader")) return false;

    if(!l->get_name) {
        LOG_FAIL("loader '%s' requires a name", l->id);
        return false;
    }

    if(!l->parse) {
        LOG_FAIL("loader '%s' requires a parse function", l->id);
        return false;
    }

    if(!l->load) {
        LOG_FAIL("loader '%s' requires a load function", l->id);
        return false;
    }

    if(rd_loader_find(l->id)) {
        LOG_WARN("loader '%s' already registered", l->id);
        return false;
    }

    LOG_DEBUG("registering loader '%s'", l->id);
    RDPlugin* plugin = rd_alloc(sizeof(*plugin));
    plugin->loader = l;
    vect_push(&rd_i_state.loaders, plugin);
    return true;
}

bool rd_register_processor(const RDProcessorPlugin* p) {
    if(!_rd_validate_plugin_with_name(p->level, p->id, p->name, "processor"))
        return false;

    if(!p->decode) {
        LOG_FAIL("processor '%s' requires a decoder", p->id);
        return false;
    }

    if(!p->emulate) {
        LOG_FAIL("processor '%s' requires an emulator", p->id);
        return false;
    }

    if(!p->ptr_size) {
        LOG_FAIL("invalid pointer-size for processor '%s'", p->id);
        return false;
    }

    if(!p->int_size) {
        LOG_FAIL("invalid integer-size for processor '%s'", p->id);
        return false;
    }

    if(rd_processor_find(p->id)) {
        LOG_WARN("processor '%s' already registered", p->id);
        return false;
    }

    LOG_DEBUG("registering processor '%s' [%s]", p->id, p->name);
    RDPlugin* plugin = rd_alloc(sizeof(*plugin));
    plugin->processor = p;
    vect_push(&rd_i_state.processors, plugin);
    return true;
}

bool rd_register_analyzer(const RDAnalyzerPlugin* a) {
    if(!_rd_validate_plugin_with_name(a->level, a->id, a->name, "analyzer"))
        return false;

    if(!a->execute) {
        LOG_FAIL("analyzer '%s' requires an executor", a->id);
        return false;
    }

    if(rd_analyzer_find(a->id)) {
        LOG_WARN("analyzer '%s' already registered", a->id);
        return false;
    }

    LOG_DEBUG("registering analyzer '%s' [%s]", a->id, a->name);
    RDPlugin* plugin = rd_alloc(sizeof(*plugin));
    plugin->analyzer = a;
    vect_push(&rd_i_state.analyzers, plugin);
    vect_sort(&rd_i_state.analyzers, _rd_analyzers_cmp);
    return true;
}

bool rd_register_command(const RDCommandPlugin* c) {
    if(!_rd_validate_plugin_with_name(c->level, c->id, c->name, "command"))
        return false;

    if(!c->execute) {
        LOG_FAIL("command '%s' requires an executor", c->id);
        return false;
    }

    if(rd_command_find(c->id)) {
        LOG_WARN("command '%s' already registered", c->id);
        return false;
    }

    LOG_DEBUG("registering command '%s' [%s]", c->id, c->name);
    RDPlugin* plugin = rd_alloc(sizeof(*plugin));
    plugin->command = c;
    vect_push(&rd_i_state.commands, plugin);
    return true;
}

bool rd_decode_bytes(const char** bytes, usize* n, RDAddress* addr,
                     const RDProcessorPlugin* p, RDDecodedInstruction* dec) {
    if(!bytes || !n || !p || !dec) return false;

    RDTestResultSlice slice = rd_test_data(*bytes, *n);
    if(rd_slice_is_empty(slice)) return false;

    const RDTestResult* tr = rd_slice_at(slice, 0);
    RDAcceptResult res = rd_accept(tr, &(RDAcceptParams){
                                           .processorplugin = p,
                                           .addressing =
                                               {
                                                   .address = *addr,
                                                   .entrypoint = *addr,
                                               },
                                       });

    if(res.status != RD_ACCEPT_OK) return false;

    RDContext* ctx = res.context;
    bool ok = rd_decode(ctx, *addr, &dec->instr);

    if(ok) {
        RDRenderer* r = rd_i_renderer_create(ctx, RD_RF_TEXT | RD_RF_NO_NAMES);
        const RDSegmentVect* segments = rd_i_db_get_segments(ctx);

        // manually craft an item
        RDListingItem item = {
            .kind = RD_LK_INSTRUCTION,
            .address = *addr,
            .segment = *vect_first(segments),
        };

        rd_i_renderer_new_row(r, &item);
        rd_i_processor_render_instruction(r, &dec->instr);
        rd_i_renderer_swap(r);
        rd_i_renderer_write_text(r, &rd_i_state.instr_text_buf);
        rd_i_renderer_destroy(r);

        dec->instr_text = rd_i_state.instr_text_buf.data;

        const char* mnem = NULL;
        if(ctx->processorplugin->get_mnemonic)
            mnem = ctx->processorplugin->get_mnemonic(&dec->instr, NULL);

        str_clear(&rd_i_state.mnem_buf);
        if(mnem) str_append(&rd_i_state.mnem_buf, mnem);

        dec->mnemonic = rd_i_state.mnem_buf.data;

        *bytes += dec->instr.length;
        *n -= dec->instr.length;
        *addr += dec->instr.length;
    }

    rd_destroy(ctx);
    return ok;
}

const RDLoaderPlugin*
rd_testresult_get_loader_plugin(const RDTestResult* self) {
    return self->loaderplugin;
}

const RDProcessorPlugin*
rd_testresult_get_processor_plugin(const RDTestResult* self) {
    return self->processorplugin;
}

const char* rd_testresult_get_loader_name(const RDTestResult* self) {
    return self->loader_name;
}

const char* rd_testresult_get_filepath(const RDTestResult* self) {
    return self->filepath;
}
