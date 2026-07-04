#include "core/context.h"
#include "io/flagsbuffer.h"
#include "plugins/analyzer.h"
#include "project.h"
#include "support/containers.h"
#include "support/utils.h"
#include <inttypes.h>
#include <miniz.h>
#include <redasm/support/logging.h>
#include <tomlc17.h>

static const char* _rd_project_write_manifest(const RDContext* ctx,
                                              RDCharVect* v) {
    RDCharVect buf = {0};

    // clang-format off
    str_append(v, "[format]\n");
    str_append(v, rd_i_format(&buf, "version = %d\n", RD_PROJECT_VERSION));
    str_append(v, "flags_compression = \"deflate\"\n");
    str_append(v, rd_i_format(&buf, "file_name = \"%s\"\n", ctx->file_name));

    str_append(v, "\n[analysis]\n");
    str_append(v, rd_i_format(&buf, "min_string = %d\n", ctx->min_string));
    str_append(v, rd_i_format(&buf, "loader = \"%s\"\n", ctx->loaderplugin->id));
    str_append(v, rd_i_format(&buf, "processor = \"%s\"\n", ctx->processorplugin->id));
    str_append(v, "analyzers = [");

    for(usize i = 0; i < vect_length(&ctx->analyzerplugins); i++) {
        if(i) str_push(v, ',');

        const RDAnalyzerItem* ai = *vect_at(&ctx->analyzerplugins, i);

        str_append(v, rd_i_format(&buf, "{ id = \"%s\", is_selected = %s, n_runs = %zu}",
                                  ai->plugin->id, 
                                  ai->is_selected ? "true" : "false",
                                  ai->n_runs));
    }

    str_append(v, "]\n");

    if(ctx->entry_point.has_value)
        str_append(v, rd_i_format(&buf, "entry_point = 0x%" PRIx64 "\n", ctx->entry_point.value));
    // clang-format on

    vect_destroy(&buf);
    return v->data;
}

static void _rd_project_add_file_buf(mz_zip_archive* zip, const char* filename,
                                     const char* data, usize n, bool* ok) {
    if(!(*ok)) return;

    *ok = mz_zip_writer_add_mem(zip, filename, data, n,
                                (mz_uint)MZ_DEFAULT_COMPRESSION);

    if(!(*ok)) RD_LOG_FAIL("cannot write '%s'", filename);
}

static void _rd_project_add_file(mz_zip_archive* zip, const char* filename,
                                 const char* inputpath, bool* ok) {
    if(!(*ok)) return;
    RDByteBuffer* b = rd_i_readfile(inputpath);

    if(!b) {
        RD_LOG_FAIL("cannot open '%s'", inputpath);
        *ok = false;
        return;
    }

    _rd_project_add_file_buf(zip, filename, (const char*)b->data,
                             rd_i_buffer_get_length((RDBuffer*)b), ok);

    rd_i_buffer_destroy((RDBuffer*)b);
}

bool rd_export(RDContext* self, const char* filepath, RDExportFormat f) {
    switch(f) {
        case RD_EXPORT_DB: {
            if(rd_i_db_export(self, filepath)) {
                RD_LOG_INFO("DB exported to %s", filepath);
                return true;
            }

            break;
        }

        default: RD_LOG_FAIL("unsupported export format %d", f); break;
    }

    return false;
}

bool rd_project_save(RDContext* self, const char* filepath) {
    RDCharVect name_buf = {0};

    if(!filepath) {
        char* stem = rd_i_get_file_stem(self->file_name);

        str_append(&name_buf, self->working_dir);
        str_push(&name_buf, RD_PATH_SEP);
        str_append(&name_buf, stem);
        str_append(&name_buf, ".rdx");

        filepath = name_buf.data;
        rd_free(stem);
    }

    char* tmpdbpath = NULL;
    RDCharVect buf = {0};
    mz_zip_archive zip = {0};
    bool ok = mz_zip_writer_init_file(&zip, filepath, 0);

    if(!ok) {
        RD_LOG_FAIL("cannot create project file '%s'", filepath);
        goto cleanup;
    }

    _rd_project_write_manifest(self, &buf);

    _rd_project_add_file_buf(&zip, RD_PROJECT_MANIFEST, buf.data,
                             vect_length(&buf), &ok);

    _rd_project_add_file_buf(
        &zip, self->file_name, (const char*)self->input->data,
        rd_i_buffer_get_length((RDBuffer*)self->input), &ok);

    tmpdbpath = rd_i_get_unique_temp_path(RD_PROJECT_DATABASE);

    if(!tmpdbpath) {
        RD_LOG_FAIL("temporary path creation failed");
        goto cleanup;
    }

    ok = rd_i_db_export(self, tmpdbpath);
    _rd_project_add_file(&zip, RD_PROJECT_DATABASE, tmpdbpath, &ok);
    remove(tmpdbpath);

    RDSegmentFull** it;
    vect_each(it, &self->db->segments) {
        RDSegmentFull* seg = *it;

        const char* flagsname =
            rd_i_format(&buf, "flags/%" PRIx64, seg->base.start_address);

        _rd_project_add_file_buf(&zip, flagsname, (const char*)seg->flags->data,
                                 rd_i_buffer_get_length((RDBuffer*)seg->flags) *
                                     sizeof(*seg->flags->data),
                                 &ok);
    }

cleanup:
    if(ok) {
        mz_zip_writer_finalize_archive(&zip);
        RD_LOG_INFO("project saved in '%s'", filepath);
    }

    rd_free(tmpdbpath);
    mz_zip_writer_end(&zip);
    vect_destroy(&buf);
    vect_destroy(&name_buf);
    return ok;
}
