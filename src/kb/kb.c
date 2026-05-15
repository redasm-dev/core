#include "kb.h"
#include "core/state.h"
#include "kb/object.h"
#include "support/containers.h"
#include "support/logging.h"
#include <redasm/allocator.h>

#define RD_KB_EXT ".toml"

static RDKBFile* _rd_kbfile_create(void) {
    return rd_alloc0(1, sizeof(RDKBFile));
}

static void _rd_kbfile_destroy(RDKBFile* self) {
    toml_free(self->toml);
    rd_free(self->name);
    rd_free(self);
}

static RDKBFile* _rd_kb_find(const char* name) {
    if(!name) return NULL;

    RDKBFile** it;
    vect_each(it, &rd_i_state.kb.files) {
        if(!strcmp(name, (*it)->name)) return *it;
    }

    return NULL;
}

static const char* _rd_kb_find_path(const char* name) {
    char** it;
    vect_each(it, &rd_i_state.kb.paths) {
        RDCharVect* p = &rd_i_state.kb.path_buf;
        str_clear(p);
        str_append(p, *it);
        str_push(p, RD_PATH_SEP);
        str_append(p, name);
        str_append(p, RD_KB_EXT);

        if(rd_i_file_exists(p->data)) return p->data;
    }

    return NULL;
}

void rd_i_kb_init(const char** kb_paths) {
    if(!kb_paths) return;

    for(const char** p = kb_paths; *p; p++) {
        char* sp = rd_strdup(*p);
        puts(sp);
        vect_push(&rd_i_state.kb.paths, sp);
    }
}

void rd_i_kb_deinit(RDKB* self) {
    RDKBFile** f;
    vect_each(f, &self->files) { _rd_kbfile_destroy(*f); }

    char** p;
    vect_each(p, &self->paths) { rd_free(*p); }
    vect_destroy(&self->paths);

    vect_destroy(&self->key_buf);
    vect_destroy(&self->path_buf);
    vect_destroy(&self->files);
}

const RDKBObject* rd_kb_load(const char* name) {
    if(!name) return NULL;

    RDKBFile* kbfile = _rd_kb_find(name);
    if(kbfile) return kbfile->root;

    const char* kb_path = _rd_kb_find_path(name);
    toml_result_t toml = toml_parse_file_ex(kb_path);

    if(!toml.ok) {
        LOG_FAIL("%s", toml.errmsg);
        return NULL;
    }

    kbfile = _rd_kbfile_create();
    kbfile->name = rd_strdup(name);
    kbfile->toml = toml;
    kbfile->root = rd_i_kb_from_datum(&kbfile->toml.toptab);

    vect_push(&rd_i_state.kb.files, kbfile);
    return kbfile->root;
}
