#include "core/context.h"
#include "core/state.h"
#include "plugins/loader.h"
#include "support/containers.h"
#include "support/utils.h"
#include "surface/renderer.h"
#include <redasm/support/logging.h>

static char* _rd_derive_path(const RDTestResult* tr,
                             const RDAcceptParams* params, const char* ext) {
    if(!strcmp(tr->filepath, ":memory:")) return rd_strdup(tr->filepath);

    const char* dir;
    char* owned_dir = NULL;

    if(params->working_dir) {
        dir = params->working_dir;
    }
    else {
        owned_dir = rd_i_get_file_path(tr->filepath);
        if(!owned_dir) return NULL;
        dir = owned_dir;
    }

    char* stem = rd_i_get_file_stem(tr->filepath);
    if(!stem) {
        rd_free(owned_dir);
        return NULL;
    }

    RDCharVect buf = {0};
    char* result =
        rd_strdup(rd_i_format(&buf, "%s%c%s.%s", dir, RD_PATH_SEP, stem, ext));
    vect_destroy(&buf);

    rd_free(stem);
    rd_free(owned_dir);
    return result;
}

void rd_init(const RDInitParams* params) { rd_i_state_init(params); }
void rd_deinit(void) { rd_i_state_deinit(); }

RDAcceptResult rd_accept(const RDTestResult* tr, const RDAcceptParams* params) {
    if(!tr || !params) return (RDAcceptResult){.status = RD_ACCEPT_FAIL};

    if(!tr->filepath) {
        RD_LOG_FAIL("filepath is NULL");
        return (RDAcceptResult){.status = RD_ACCEPT_FAIL};
    }

    char* workingdir = rd_i_get_file_path(tr->filepath);
    if(!workingdir) {
        RD_LOG_FAIL("cannot extract filepath from '%s'", tr->filepath);
        return (RDAcceptResult){.status = RD_ACCEPT_FAIL};
    }

    const char* filename = rd_i_get_file_name(tr->filepath);
    if(!filename) {
        RD_LOG_FAIL("cannot extract filename from '%s'", tr->filepath);
        return (RDAcceptResult){.status = RD_ACCEPT_FAIL};
    }

    RDAcceptResult res = {.status = RD_ACCEPT_FAIL};
    RDTestResult** it;
    char* dbpath = NULL;

    const RDProcessorPlugin* pplugin =
        params->processorplugin ? params->processorplugin : tr->processorplugin;

    if(!pplugin) {
        RD_LOG_FAIL("processor plugin not set for loader '%s'",
                    tr->loaderplugin->id);
        goto cleanup;
    }

    if(params->mode == RD_AM_PROJECT) {
        char* rdxpath = _rd_derive_path(tr, params, "rdx");

        if(rdxpath && rd_i_file_exists(rdxpath)) {
            res = rd_project_load(rdxpath, workingdir);

            if(res.status == RD_ACCEPT_OK) // project loads input from .rdx
                rd_i_buffer_destroy((RDBuffer*)tr->input_buffer);

            rd_free(rdxpath);
            goto cleanup;
        }

        rd_free(rdxpath);
    }

    dbpath = _rd_derive_path(tr, params, "db");
    if(!dbpath) goto cleanup;

    if(!rd_i_path_is_writable(dbpath)) {
        res.status = RD_ACCEPT_FAIL_WRITE;
        goto cleanup;
    }

    if(rd_i_file_exists(dbpath)) remove(dbpath);

    res.context =
        rd_i_context_create(tr->loaderplugin, pplugin, tr->input_buffer,
                            workingdir, filename, dbpath);
    if(!res.context) goto cleanup;

    res.context->min_string =
        params->min_string ? params->min_string : RD_MIN_STRING_LENGTH;
    res.context->addressing = params->addressing;

    rd_reader_seek(res.context->input_reader, 0);

    if(res.context->loaderplugin->load(tr->loader, res.context)) {
        res.status = RD_ACCEPT_OK;
        RD_LOG_INFO("selected loader '%s' and processor '%s'",
                    res.context->loaderplugin->id,
                    res.context->processorplugin->id);
    }
    else {
        rd_destroy(res.context);
        res.context = NULL;
        res.status = RD_ACCEPT_FAIL;
        goto cleanup;
    }

    rd_reader_seek(res.context->input_reader, 0);

cleanup:
    if(res.status != RD_ACCEPT_FAIL_WRITE) {
        vect_each(it, &rd_i_state.tests) rd_i_testresult_destroy(*it);
        vect_clear(&rd_i_state.tests);
    }

    rd_free(dbpath);
    rd_free(workingdir);
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

const RDScratchBuffer* rd_encode_instruction(const char* s, RDAddress address,
                                             const RDProcessorPlugin* p,
                                             const char** errmsg) {
    if(!p) return NULL;

    if(!rd_i_state.encode_ctx || rd_i_state.encode_ctx->processorplugin != p) {
        rd_destroy(rd_i_state.encode_ctx);
        rd_i_state.encode_ctx = NULL;

        static const char DUMMY_DATA[] = {0x00, 0x00, 0x00, 0x00};

        RDTestResultSlice slice =
            rd_test_data(DUMMY_DATA, rd_count_of(DUMMY_DATA));

        if(rd_slice_is_empty(slice)) {
            rd_scratch_clear(&rd_i_state.encode_buf);
            RD_LOG_FAIL_TO(&rd_i_state.encode_buf,
                           "failed to create encoding context for '%s'", p->id);
            if(errmsg) *errmsg = rd_scratch_data(&rd_i_state.encode_buf);
            return NULL;
        }

        const RDTestResult* tr = rd_slice_at(slice, 0);
        RDAcceptResult res = rd_accept(tr, &(RDAcceptParams){
                                               .processorplugin = p,
                                               .addressing =
                                                   {
                                                       .address = address,
                                                       .entrypoint = address,
                                                   },
                                           });

        if(res.status != RD_ACCEPT_OK) return NULL;
        rd_i_state.encode_ctx = res.context;
    }

    RDContext* ctx = rd_i_state.encode_ctx;
    if(!ctx) return NULL;

    bool ok = rd_encode(ctx, address, s, &rd_i_state.encode_buf);
    if(!ok && errmsg) *errmsg = rd_scratch_data(&rd_i_state.encode_buf);
    return ok ? &rd_i_state.encode_buf : NULL;
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
        const RDSegmentFullVect* segments = rd_i_db_get_segments(ctx);

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
