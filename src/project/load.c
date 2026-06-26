#include "core/context.h"
#include "core/worker.h"
#include "io/flagsbuffer.h"
#include "listing/builder.h"
#include "project.h"
#include "support/containers.h"
#include "support/logging.h"
#include "support/tomlschema.h"
#include <inttypes.h>
#include <miniz.h>
#include <tomlc17.h>

typedef struct RDProjectManifest {
    int64_t version;
    const char* file_name;

    const RDLoaderPlugin* loaderplugin;
    const RDProcessorPlugin* processorplugin;

    int min_string;

    struct {
        RDAddress value;
        bool has_value;
    } entry_point;

    RDAnalyzerItemVect analyzers;

    toml_result_t toml;
    char* raw;
} RDProjectManifest;

typedef struct RDProjectPaths {
    char* working_dir;
    char* db_path;
} RDProjectPaths;

// clang-format off
static const char* flags_compression_schema[] = {
    "deflate",
    NULL,
};

static const RDTomlSchema ANALYZER_SCHEMA[] = {
    {.key = "id", .type = TOML_STRING},
    {.key = "is_selected", .type = TOML_BOOLEAN},
    {.key = "n_runs", .type = TOML_INT64},
    {0},
};

static const RDTomlSchema MANIFEST_SCHEMA[] = {
    {.key = "format.version", .type = TOML_INT64},
    {.key = "format.flags_compression", .type = TOML_STRING, .string_values = flags_compression_schema},
    {.key = "format.file_name", .type = TOML_STRING},

    {.key = "analysis.min_string", .type = TOML_INT64},
    {.key = "analysis.loader", .type = TOML_STRING},
    {.key = "analysis.processor", .type = TOML_STRING},
    {.key = "analysis.analyzers", .type = TOML_ARRAY, .array_type = ANALYZER_SCHEMA},
    {.key = "analysis.entry_point", .type = TOML_INT64, .optional = true},

    {0},
};
// clang-format on

static void _rd_project_manifest_destroy(RDProjectManifest* m) {
    rd_i_analyzeritemvect_destroy(&m->analyzers);
    toml_free(m->toml);
    rd_free(m->raw);
}

static void _rd_project_paths_destroy(RDProjectPaths* p) {
    rd_free(p->db_path);
    rd_free(p->working_dir);
}

static bool _rd_project_resolve_paths(const char* filepath,
                                      const char* workingdir,
                                      const RDProjectManifest* manifest,
                                      RDProjectPaths* out,
                                      RDAcceptResult* res) {
    if(!workingdir) {
        out->working_dir = rd_i_get_file_path(filepath);

        if(!out->working_dir) {
            LOG_FAIL("cannot get file path for '%s'", filepath);
            return false;
        }
    }
    else
        out->working_dir = rd_strdup(workingdir);

    char* stem = rd_i_get_file_stem(manifest->file_name);

    if(!stem) {
        LOG_FAIL("cannot get file stem for '%s'", manifest->file_name);
        return false;
    }

    RDCharVect buf = {0};

    out->db_path = rd_strdup(
        rd_i_format(&buf, "%s%c%s.db", out->working_dir, RD_PATH_SEP, stem));

    vect_destroy(&buf);
    rd_free(stem);

    if(!rd_i_path_is_writable(out->db_path)) {
        res->status = RD_ACCEPT_FAIL_WRITE;
        return false;
    }

    return true;
}

static bool _rd_project_read_manifest(mz_zip_archive* zip,
                                      RDProjectManifest* out) {
    size_t n = 0;
    char* manifest_buf = (char*)mz_zip_reader_extract_file_to_heap(
        zip, RD_PROJECT_MANIFEST, &n, 0);

    if(!manifest_buf || !n) {
        LOG_FAIL("project manifest not found");
        return false;
    }

    // tomlc17 seems to ignore length and wants a null terminator
    out->raw = rd_alloc0(n + 1, sizeof(char));
    memcpy(out->raw, manifest_buf, n);
    mz_free(manifest_buf);

    out->toml = toml_parse(out->raw, (int)n);

    if(!out->toml.ok) {
        LOG_FAIL("invalid project manifest: %s", out->toml.errmsg);
        return false;
    }

    if(!rd_i_toml_validate_schema(out->toml.toptab, MANIFEST_SCHEMA))
        return false;

    out->version = toml_seek(out->toml.toptab, "format.version").u.int64;

    if(out->version != RD_PROJECT_VERSION) {
        LOG_FAIL("expected %d as project version, got %" PRId64,
                 RD_PROJECT_VERSION, out->version);
        return false;
    }

    out->file_name = toml_seek(out->toml.toptab, "format.file_name").u.s;

    out->min_string =
        (int)toml_seek(out->toml.toptab, "analysis.min_string").u.int64;

    const char* loader_id = toml_seek(out->toml.toptab, "analysis.loader").u.s;
    assert(loader_id);

    out->loaderplugin = rd_loader_find(loader_id);

    if(!out->loaderplugin) {
        LOG_FAIL("loader '%s' not found", loader_id);
        return false;
    }

    const char* processor_id =
        toml_seek(out->toml.toptab, "analysis.processor").u.s;
    assert(processor_id);

    out->processorplugin = rd_processor_find(processor_id);

    if(!out->processorplugin) {
        LOG_FAIL("processor '%s' not found", processor_id);
        return false;
    }

    toml_datum_t ep = toml_seek(out->toml.toptab, "analysis.entry_point");
    out->entry_point.has_value = ep.type != TOML_UNKNOWN;
    out->entry_point.value = (RDAddress)ep.u.int64;

    toml_datum_t analyzers = toml_seek(out->toml.toptab, "analysis.analyzers");

    for(int i = 0; i < analyzers.u.arr.size; i++) {
        toml_datum_t elem = analyzers.u.arr.elem[i];
        const char* id = toml_seek(elem, "id").u.s;
        const RDAnalyzerPlugin* plugin = rd_analyzer_find(id);

        if(!plugin) {
            LOG_FAIL("analyzer '%s' not found", id);
            return false;
        }

        RDAnalyzerItem* ai = rd_alloc(sizeof(*ai));
        ai->plugin = plugin;
        ai->is_selected = toml_seek(elem, "is_selected").u.boolean;
        ai->n_runs = (usize)toml_seek(elem, "n_runs").u.int64;
        vect_push(&out->analyzers, ai);
    }

    return true;
}

static bool _rd_project_extract_db(mz_zip_archive* zip,
                                   const RDProjectPaths* paths) {
    if(!mz_zip_reader_extract_file_to_file(zip, RD_PROJECT_DATABASE,
                                           paths->db_path, 0)) {
        LOG_FAIL("cannot extract project database to '%s'", paths->db_path);
        return false;
    }
    return true;
}

static RDByteBuffer*
_rd_project_extract_input(mz_zip_archive* zip,
                          const RDProjectManifest* manifest) {
    size_t n = 0;
    void* data =
        mz_zip_reader_extract_file_to_heap(zip, manifest->file_name, &n, 0);

    if(!data) {
        LOG_FAIL("cannot read project input file '%s'", manifest->file_name);
        return NULL;
    }

    RDByteBuffer* input = rd_i_fromdata(data, n);
    mz_free(data);
    return input;
}

static RDContext* _rd_project_create_context(mz_zip_archive* zip,
                                             RDByteBuffer* input,
                                             RDProjectManifest* manifest,
                                             const RDProjectPaths* paths) {
    RDContext* ctx = rd_i_context_create(
        manifest->loaderplugin, manifest->processorplugin, input,
        paths->working_dir, manifest->file_name, paths->db_path);

    if(!ctx) return NULL;

    rd_i_db_load_segments(ctx);

    RDCharVect buf = {0};

    RDSegmentFull** seg;
    vect_each(seg, &ctx->db->segments) {
        const char* flagsname =
            rd_i_format(&buf, "flags/%" PRIx64, (*seg)->base.start_address);

        size_t n = 0;
        char* flags =
            (char*)mz_zip_reader_extract_file_to_heap(zip, flagsname, &n, 0);

        if(!flags ||
           n != rd_flagsbuffer_get_length((*seg)->flags) * sizeof(RDFlags)) {
            LOG_FAIL("flags mismatch for segment '%s'", (*seg)->base.name);
            mz_free(flags);
            goto fail;
        }

        // bypass buffer write behavior: write flags directly
        memcpy((char*)(*seg)->flags->data, flags, n);

        mz_free(flags);
    }

    vect_destroy(&buf);

    ctx->entry_point.has_value = manifest->entry_point.has_value;
    ctx->entry_point.value = manifest->entry_point.value;
    mem_swap(RDAnalyzerItemVect, &ctx->analyzerplugins, &manifest->analyzers);

    ctx->engine.step = RD_WS_DONE;
    rd_i_db_load(ctx);
    rd_i_listing_build(ctx);

    return ctx;

fail:
    vect_destroy(&buf);
    rd_destroy(ctx);
    return NULL;
}

RDAcceptResult rd_project_load(const char* filepath, const char* workingdir) {
    if(!filepath) return (RDAcceptResult){.status = RD_ACCEPT_FAIL};

    mz_zip_archive zip = {0};

    if(!mz_zip_reader_init_file(&zip, filepath, 0)) {
        LOG_FAIL("cannot open project '%s'", filepath);
        return (RDAcceptResult){.status = RD_ACCEPT_FAIL};
    }

    RDAcceptResult res = {.status = RD_ACCEPT_FAIL};
    RDProjectManifest manifest = {0};
    RDProjectPaths paths = {0};
    RDByteBuffer* input = NULL;

    if(!_rd_project_read_manifest(&zip, &manifest)) goto cleanup;

    if(!_rd_project_resolve_paths(filepath, workingdir, &manifest, &paths,
                                  &res))
        goto cleanup;

    if(!_rd_project_extract_db(&zip, &paths)) goto cleanup;

    input = _rd_project_extract_input(&zip, &manifest);
    if(!input) goto cleanup;

    res.context = _rd_project_create_context(&zip, input, &manifest, &paths);
    if(!res.context) goto cleanup;

    res.status = RD_ACCEPT_OK;
    LOG_INFO("loading project '%s'", filepath);

cleanup:
    if(res.status != RD_ACCEPT_OK) {
        rd_destroy(res.context);
        res.context = NULL;
    }

    _rd_project_paths_destroy(&paths);
    _rd_project_manifest_destroy(&manifest);
    mz_zip_reader_end(&zip);
    return res;
}
