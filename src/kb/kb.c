#include "kb.h"
#include "core/context.h"
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

static RDKBFile* _rd_kb_find(const RDContext* ctx, const char* name) {
    if(!name) return NULL;

    RDKBFile** it;
    vect_each(it, &ctx->kb->files) {
        if(!strcmp(name, (*it)->name)) return *it;
    }

    return NULL;
}

static const char* _rd_kb_find_path(const char* name) {
    char** it;
    vect_each(it, &rd_i_state.kb_paths) {
        RDCharVect* p = &rd_i_state.kb_path_buf;
        str_clear(p);
        str_append(p, *it);
        str_push(p, RD_PATH_SEP);
        str_append(p, name);
        str_append(p, RD_KB_EXT);

        if(rd_i_file_exists(p->data)) return p->data;
    }

    return NULL;
}

static const char* _rd_kb_get_param(const RDKBObject* obj, RDType* t,
                                    const RDContext* ctx) {
    if(!rd_i_kb_validate_param(obj)) return NULL;

    const RDProcessorPlugin* plugin = ctx->processorplugin;
    i64 count = 0;
    i64 mod = RD_TYPE_NONE;

    const char* name = rd_kbobject_get_str(obj, "name");
    assert(name);

    t->name = rd_kbobject_get_str(obj, "type");
    assert(t->name);

    rd_kbobject_get_int(obj, "count", &count);
    t->count = count;

    rd_kbobject_get_int(obj, "mod", &mod);
    t->mod = mod;

    if(!strcmp(t->name, "cptr")) {
        if(plugin->code_ptr_size) {
            t->name = rd_integral_from_size(plugin->code_ptr_size);
            panic_if(!t->name, "cannot get code-pointer size");
            mod = RD_TYPE_PTR; // always pointer
        }
        else
            t->name = "ptr";
    }

    if(!strcmp(t->name, "ptr")) {
        t->name = rd_integral_from_size(plugin->ptr_size);
        panic_if(!t->name, "cannot get pointer size");
        mod = RD_TYPE_PTR; // always pointer
    }

    return name;
}

static bool _rd_kb_load_struct(const char* name, const RDKBObject* def,
                               RDContext* ctx) {
    RDTypeDef* tdef = rd_typedef_create_struct(name, ctx);

    const RDKBObject* members = rd_kbobject_get_array(def, "members");
    assert(members);

    const RDKBObject* m;
    rd_kbobject_each(m, members) {
        RDType t;
        const char* name = _rd_kb_get_param(m, &t, ctx);

        if(!name) {
            rd_typedef_destroy(tdef);
            return false;
        }

        rd_typedef_add_member(tdef, t.name, name, t.count, t.mod, ctx);
    }

    return rd_typedef_register(tdef, ctx);
}

static void rd_i_db_apply_noret(RDContext* ctx) {
    const char** it;
    vect_each(it, &ctx->kb->noret_names) {
        RDAddress address;
        if(!rd_get_address(ctx, *it, &address)) continue;

        const RDSegmentFull* seg = rd_i_db_find_segment(ctx, address);
        assert(seg);

        usize idx = rd_i_address2index(seg, address);
        rd_i_set_noret(ctx, seg, idx);
    }
}

void rd_i_kb_paths_init(const char** kb_paths) {
    if(!kb_paths) return;

    for(const char** p = kb_paths; *p; p++) {
        char* sp = rd_strdup(*p);
        puts(sp);
        vect_push(&rd_i_state.kb_paths, sp);
    }
}

void rd_i_kb_paths_deinit(RDPathVect* self) {
    char** p;
    vect_each(p, self) { rd_free(*p); }
    vect_destroy(self);
}

RDKB* rd_i_kb_create(void) { return rd_alloc0(1, sizeof(RDKB)); }

void rd_i_kb_destroy(RDKB* self) {
    RDKBFile** f;
    vect_each(f, &self->files) { _rd_kbfile_destroy(*f); }

    vect_destroy(&self->noret_names);
    vect_destroy(&self->files);
    rd_free(self);
}

void rd_i_kb_add_noret(RDContext* ctx, const char* name) {
    assert(name);

    usize idx =
        vect_lower_bound(&ctx->kb->noret_names, name, rd_i_strcmp_intern_pred);

    if(idx == vect_length(&ctx->kb->noret_names) ||
       name != *vect_at(&ctx->kb->noret_names, idx)) {
        vect_ins(&ctx->kb->noret_names, idx, name);
    }
}

bool rd_i_kb_is_noret(const RDContext* self, const char* name) {
    assert(name);

    usize idx =
        vect_bsearch(&self->kb->noret_names, name, rd_i_strcmp_key_pred);
    return idx < vect_length(&self->kb->noret_names);
}

const RDKBObject* rd_kb_load(RDContext* ctx, const char* kb) {
    if(!kb) return NULL;

    RDKBFile* kbfile = _rd_kb_find(ctx, kb);
    if(kbfile) return kbfile->root;

    const char* kb_path = _rd_kb_find_path(kb);
    if(!kb_path) return NULL;

    toml_result_t toml = toml_parse_file_ex(kb_path);

    if(!toml.ok) {
        LOG_FAIL("%s", toml.errmsg);
        return NULL;
    }

    kbfile = _rd_kbfile_create();
    kbfile->name = rd_strdup(kb);
    kbfile->toml = toml;
    kbfile->root = rd_i_kb_from_datum(&kbfile->toml.toptab);

    LOG_INFO("loading KB '%s'", kb);
    vect_push(&ctx->kb->files, kbfile);
    return kbfile->root;
}

bool rd_kb_load_types(RDContext* ctx, const char* kb) {
    const RDKBObject* kbfile = rd_kb_load(ctx, kb);
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

bool rd_kb_load_functions(RDContext* ctx, const char* kb) {
    const RDKBObject* kbfile = rd_kb_load(ctx, kb);
    if(!kbfile) return false;

    const RDKBObject* functions = rd_kbobject_get_table(kbfile, "functions");
    if(!functions) return false;

    const char* name;
    const RDKBObject* f;
    rd_kbobject_each_pair(name, f, functions) {
        if(!rd_i_kb_validate_function(f)) continue;

        RDTypeDef* tdef = rd_typedef_create_func(name, ctx);
        const RDKBObject* args = rd_kbobject_get_array(f, "args");
        assert(args);

        bool is_noret = false;
        rd_kbobject_get_bool(f, "noret", &is_noret);
        rd_typedef_set_noret(tdef, is_noret);

        // TODO: davide - fully express type
        const char* ret = rd_kbobject_get_str(f, "ret");
        assert(ret);
        rd_typedef_set_ret(tdef, ret, 0, RD_TYPE_NONE, ctx);

        const RDKBObject* a;
        rd_kbobject_each(a, args) {
            RDType t;
            const char* name = _rd_kb_get_param(a, &t, ctx);

            if(!name) {
                rd_typedef_destroy(tdef);
                return false;
            }

            rd_typedef_add_arg(tdef, t.name, name, t.count, t.mod, ctx);
        }

        rd_typedef_register(tdef, ctx);
    }

    rd_i_db_apply_noret(ctx);
    return true;
}
