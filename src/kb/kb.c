#include "kb.h"
#include "core/state.h"
#include "kb/object.h"
#include "kb/schema.h"
#include "support/containers.h"
#include "support/error.h"
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

static void _rd_kb_load_struct(const char* name, const RDKBObject* def,
                               RDContext* ctx) {
    RDTypeDef* tdef = rd_typedef_create_struct(name, ctx);

    const RDKBObject* members = rd_kbobject_get_array(def, "members");
    assert(members);

    const RDKBObject* m;
    rd_kbobject_each(m, members) {
        if(!rd_i_kb_validate_member(m)) {
            rd_typedef_destroy(tdef);
            return;
        }

        i64 count = 0;
        i64 mod = RD_TYPE_NONE;

        const char* type = rd_kbobject_get_str(m, "type");
        const char* name = rd_kbobject_get_str(m, "name");
        rd_kbobject_get_int(m, "count", &count);
        rd_kbobject_get_int(m, "mod", &mod);

        rd_typedef_add_member(tdef, type, name, count, (RDTypeModifier)mod,
                              ctx);
    }

    rd_typedef_register(tdef, ctx);
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
    vect_destroy(&self->schema_buf);
    vect_destroy(&self->files);
}

const RDKBObject* rd_kb_load(const char* kb) {
    if(!kb) return NULL;

    RDKBFile* kbfile = _rd_kb_find(kb);
    if(kbfile) return kbfile->root;

    const char* kb_path = _rd_kb_find_path(kb);
    toml_result_t toml = toml_parse_file_ex(kb_path);

    if(!toml.ok) {
        LOG_FAIL("%s", toml.errmsg);
        return NULL;
    }

    kbfile = _rd_kbfile_create();
    kbfile->name = rd_strdup(kb);
    kbfile->toml = toml;
    kbfile->root = rd_i_kb_from_datum(&kbfile->toml.toptab);

    vect_push(&rd_i_state.kb.files, kbfile);
    return kbfile->root;
}

bool rd_kb_load_types(const char* kb, RDContext* ctx) {
    const RDKBObject* kbfile = rd_kb_load(kb);
    if(!kbfile) return false;

    const RDKBObject* types = rd_kbobject_get_table(kbfile, "types");
    if(!types) return false;

    const char* name;
    const RDKBObject* def;

    rd_kbobject_each_pair(name, def, types) {
        if(!rd_i_kb_validate_type(def)) continue;

        const char* kind = rd_kbobject_get_str(def, "kind");

        if(!strcmp(kind, "struct"))
            _rd_kb_load_struct(name, def, ctx);
        else
            unreachable();
    }

    return true;
}
