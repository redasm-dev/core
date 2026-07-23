#include "kb.h"
#include "core/context.h"
#include "core/state.h"
#include "kb/object.h"
#include "kb/schema.h"
#include "support/containers.h"
#include "support/error.h"
#include "support/tomlschema.h"
#include <errno.h>
#include <redasm/allocator.h>
#include <redasm/support/logging.h>

#define RD_KB_EXT ".toml"

static int _rd_kb_ordinal_modules_cmp_pred(const void* key, const void* v) {
    const char* modname = *(const char**)key;
    const RDKBOrdinalModule* m = (const RDKBOrdinalModule*)v;
    return rd_stricmp(modname, m->module);
}

static int _rd_kb_ordinal_cmp_pred(const void* key, const void* v) {
    u32 ordinal = *(u32*)key;
    const RDKBOrdinal* ord = (const RDKBOrdinal*)v;
    if(ordinal < ord->ordinal) return -1;
    if(ordinal > ord->ordinal) return 1;
    return 0;
}

static RDKBFile* _rd_kbfile_create(void) {
    return rd_alloc0(1, sizeof(RDKBFile));
}

static void _rd_kbfile_destroy(RDKBFile* self) {
    toml_free(self->toml);
    rd_free(self->name);
    rd_free(self);
}

static RDKBFile* _rd_kb_find_file(const RDContext* ctx, const char* name) {
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

static RDKBOrdinalModule* _rd_kb_find_ordinal_module(const RDContext* ctx,
                                                     const char* module,
                                                     usize* out_idx) {
    if(!module) return NULL;

    usize idx = vect_lower_bound(&ctx->kb->ordinal_modules, &module,
                                 _rd_kb_ordinal_modules_cmp_pred);

    if(out_idx) *out_idx = idx;

    if(idx < vect_length(&ctx->kb->ordinal_modules) &&
       !rd_stricmp(module, vect_at(&ctx->kb->ordinal_modules, idx)->module))
        return vect_at(&ctx->kb->ordinal_modules, idx);

    return NULL;
}

static RDKBOrdinalModule* _rd_kb_check_ordinal_module(RDContext* ctx,
                                                      const char* module) {
    if(!module) return NULL;

    usize idx;
    RDKBOrdinalModule* mod = _rd_kb_find_ordinal_module(ctx, module, &idx);
    if(mod) return mod;

    vect_ins(&ctx->kb->ordinal_modules, idx,
             (RDKBOrdinalModule){
                 .module = rd_i_strpool_intern(&ctx->strings, module),
             });

    return vect_at(&ctx->kb->ordinal_modules, idx);
}

static const char* _rd_kb_get_param(const RDKBObject* obj, RDType* t,
                                    const RDContext* ctx, bool is_ret) {
    *t = (RDType){.mod = RD_TYPE_NONE};

    i64 count = 0;
    const char* tname = rd_kbobject_get_str(obj, "type");
    assert(tname);

    if(!strcmp(tname, "void")) {
        if(!is_ret) {
            RD_LOG_FAIL("void type unallowed");
            return NULL;
        }

        rd_type_init_void(t);

        // return type is nameless, prefer empty string instead of NULL
        return "";
    }

    bool is_type_size = !strcmp(tname, "size");
    bool is_type_cptr = !strcmp(tname, "cptr");
    bool is_type_ptr = !strcmp(tname, "ptr");

    rd_kbobject_get_int(obj, "count", &count);
    t->count = (usize)count;

    const char* mod_str = rd_kbobject_get_str(obj, "mod");
    bool is_mod_str_cptr = mod_str && !strcmp(mod_str, "cptr");
    bool is_mod_str_ptr = mod_str && !strcmp(mod_str, "ptr");

    if(is_type_cptr) {
        const RDProcessorPlugin* plugin = ctx->processorplugin;

        if(plugin->code_ptr_size) {
            t->def =
                rd_integral_typedef_from_size(rd_get_code_ptr_size(ctx), ctx);
            panic_if(!t->def, "cannot get code-pointer size");
            t->mod = RD_TYPE_CPTR;
        }
        else
            is_type_ptr = true;
    }

    if(is_type_ptr) {
        t->def = rd_integral_typedef_from_size(rd_get_ptr_size(ctx), ctx);
        panic_if(!t->def, "cannot get pointer size");
        t->mod = RD_TYPE_PTR;
    }

    if(is_type_size) {
        t->def = rd_integral_typedef_from_size(rd_get_int_size(ctx), ctx);
        panic_if(!t->def, "cannot get integer size");
    }

    if(t->mod == RD_TYPE_NONE) {
        if(is_mod_str_cptr)
            t->mod = RD_TYPE_CPTR;
        else if(is_mod_str_ptr)
            t->mod = RD_TYPE_PTR;
    }

    if(!t->def) t->def = rd_i_typedef_find(ctx, tname);

    if(!t->def) {
        RD_LOG_FAIL("cannot find type '%s'", tname);
        return NULL;
    }

    return rd_kbobject_get_str(obj, "name");
}

static void _rd_kb_apply_noret(RDContext* ctx) {
    const char** it;
    vect_each(it, &ctx->kb->noret_names) {
        RDAddress address;
        if(!rd_get_address(ctx, *it, &address)) continue;
        rd_i_set_noret(ctx, address);
    }
}

static void _rd_kb_load_dependencies(const RDKBObject* root, RDContext* ctx) {
    const RDKBObject* manifest = rd_kbobject_get_table(root, "manifest");
    if(!manifest) return;

    const RDKBObject* dependencies =
        rd_kbobject_get_array(manifest, "dependencies");
    if(!dependencies) return;

    const RDKBObject* dep;
    rd_kbobject_each(dep, dependencies) {
        const char* dep_path = rd_kbobject_to_str(dep);
        if(dep_path) rd_kb_load(ctx, dep_path);
    }
}

static bool _rd_kb_load_compound(const RDKBObject* root, RDContext* ctx,
                                 const char* kind) {
    const RDKBObject* types = rd_kbobject_get_table(root, kind);
    if(!types) return false;

    const char* name;
    const RDKBObject* def;
    rd_kbobject_each_pair(name, def, types) {
        if(!rd_i_kb_validate_compound(def)) continue;

        RDTypeDef* tdef = rd_typedef_create_struct(name, ctx);
        assert(tdef);

        const RDKBObject* members = rd_kbobject_get_array(def, "members");
        assert(members);

        const RDKBObject* m;
        rd_kbobject_each(m, members) {
            RDType t;
            const char* param_name = _rd_kb_get_param(m, &t, ctx, false);

            if(!param_name) {
                rd_typedef_destroy(tdef);
                return false;
            }

            rd_typedef_add_member(tdef, t.def->name, param_name, t.count, t.mod,
                                  ctx);
        }

        rd_typedef_register(tdef, ctx);
    }

    return true;
}

static bool _rd_kb_load_enums(const RDKBObject* root, RDContext* ctx) {
    const RDKBObject* enums = rd_kbobject_get_table(root, "enums");
    if(!enums) return false;

    const char* name;
    const RDKBObject* e;
    rd_kbobject_each_pair(name, e, enums) {
        if(!rd_i_kb_validate_enum(e)) continue;

        const char* base_type = rd_kbobject_get_str(e, "base_type");
        assert(base_type);

        RDTypeDef* tdef = rd_typedef_create_enum(name, base_type, ctx);

        const RDKBObject* members = rd_kbobject_get_array(e, "members");
        assert(members);

        const RDKBObject* m;
        rd_kbobject_each(m, members) {
            const char* m_name = rd_kbobject_get_str(m, "name");
            i64 m_value;
            bool value_ok = rd_kbobject_get_int(m, "value", &m_value);

            if(!m_name || !value_ok) {
                rd_typedef_destroy(tdef);
                return false;
            }

            rd_typedef_add_enumval(tdef, m_name, m_value, ctx);
        }

        rd_typedef_register(tdef, ctx);
    }

    return true;
}

static bool _rd_kb_load_functions(const RDKBObject* root, RDContext* ctx) {
    const RDKBObject* functions = rd_kbobject_get_table(root, "functions");
    if(!functions) return false;

    const char* name;
    const RDKBObject* f;
    rd_kbobject_each_pair(name, f, functions) {
        if(!rd_i_kb_validate_function(f)) continue;

        RDTypeDef* tdef = rd_typedef_create_func(name, ctx);
        bool is_noret = false;
        rd_kbobject_get_bool(f, "noret", &is_noret);
        rd_typedef_set_noret(tdef, is_noret);

        const RDKBObject* ret = rd_kbobject_get(f, "ret");
        assert(ret);

        RDType ret_type;
        _rd_kb_get_param(ret, &ret_type, ctx, true);

        if(rd_type_is_void(&ret_type)) {
            rd_typedef_set_ret(tdef, NULL, 0, RD_TYPE_NONE, ctx);
        }
        else {
            rd_typedef_set_ret(tdef, ret_type.def->name, ret_type.count,
                               ret_type.mod, ctx);
        }

        const RDKBObject* args = rd_kbobject_get_array(f, "args");
        assert(args);

        const RDKBObject* a;
        rd_kbobject_each(a, args) {
            RDType t;
            const char* arg_name = _rd_kb_get_param(a, &t, ctx, false);

            if(!arg_name) {
                rd_typedef_destroy(tdef);
                return false;
            }

            rd_typedef_add_arg(tdef, t.def->name, arg_name, t.count, t.mod,
                               ctx);
        }

        rd_typedef_register(tdef, ctx);
    }

    _rd_kb_apply_noret(ctx);
    return true;
}

static bool _rd_kb_load_ordinals(const RDKBObject* root, RDContext* ctx) {
    const RDKBObject* ordinals = rd_kbobject_get_table(root, "ordinals");
    if(!ordinals) return false;

    const char* modname;
    const RDKBObject* ord_list;
    rd_kbobject_each_pair(modname, ord_list, ordinals) {
        RDKBOrdinalModule* mod = _rd_kb_check_ordinal_module(ctx, modname);

        const char* ord_str;
        const RDKBObject* ord;
        rd_kbobject_each_pair(ord_str, ord, ord_list) {
            errno = 0;
            u32 ord_val = (u32)strtoul(ord_str, NULL, 10);

            if(errno != 0) {
                RD_LOG_WARN("invalid ordinal '%s' for module '%s', skipping...",
                            ord_str, modname);
                continue;
            }

            const char* func_name = rd_kbobject_to_str(ord);

            if(!func_name) {
                RD_LOG_WARN(
                    "ordinal-name must be '%s', got '%s' for module '%s', "
                    "skipping...",
                    rd_i_toml_type_str(TOML_STRING),
                    rd_i_toml_type_str(rd_i_kbobject_toml_type(ord)), modname);
                continue;
            }

            usize idx = vect_lower_bound(&mod->ordinals, &ord_val,
                                         _rd_kb_ordinal_cmp_pred);

            if(idx < vect_length(&mod->ordinals) &&
               vect_at(&mod->ordinals, idx)->ordinal == ord_val) {
                RD_LOG_WARN(
                    "duplicate ordinal '%s', ' for module '%s', skipping...",
                    ord_str, modname);
                continue;
            }

            vect_ins(&mod->ordinals, idx,
                     (RDKBOrdinal){
                         .ordinal = ord_val,
                         .name = rd_i_strpool_intern(&ctx->strings, func_name),
                     });
        }
    }

    return true;
}

void rd_i_kb_paths_init(const char** kb_paths) {
    if(!kb_paths) return;

    for(const char** p = kb_paths; *p; p++) {
        char* sp = rd_strdup(*p);
        assert(sp && "invalid searchpath");
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

    RDKBOrdinalModule* ord_modules;
    vect_each(ord_modules, &self->ordinal_modules) {
        vect_destroy(&ord_modules->ordinals);
    }

    vect_destroy(&self->ordinal_modules);
    vect_destroy(&self->noret_names);
    vect_destroy(&self->files);
    rd_free(self);
}

void rd_i_kb_add_noret(RDContext* ctx, const char* name) {
    assert(name);

    usize idx =
        vect_lower_bound(&ctx->kb->noret_names, name, rd_i_strcmp_key_pred);

    if(idx == vect_length(&ctx->kb->noret_names) ||
       name != *vect_at(&ctx->kb->noret_names, idx)) {
        vect_ins(&ctx->kb->noret_names, idx, name);
    }
}

bool rd_i_kb_is_noret(const RDContext* ctx, const char* name) {
    assert(name);

    usize idx = vect_bsearch(&ctx->kb->noret_names, name, rd_i_strcmp_key_pred);
    return idx < vect_length(&ctx->kb->noret_names);
}

const char* rd_i_kb_find_ordinal_name(RDContext* ctx, const char* module,
                                      u32 ordinal) {
    const RDKBOrdinalModule* mod =
        _rd_kb_find_ordinal_module(ctx, module, NULL);
    if(!mod) return NULL;

    usize idx = vect_bsearch(&mod->ordinals, &ordinal, _rd_kb_ordinal_cmp_pred);

    if(idx < vect_length(&mod->ordinals))
        return vect_at(&mod->ordinals, idx)->name;

    return NULL;
}

const RDKBObject* rd_kb_load(RDContext* ctx, const char* kb) {
    if(!kb) return NULL;

    RDKBFile* kbfile = _rd_kb_find_file(ctx, kb);
    if(kbfile) return kbfile->root;

    const char* kb_path = _rd_kb_find_path(kb);

    if(!kb_path) {
        RD_LOG_FAIL("KB '%s' not found", kb);
        return NULL;
    }

    toml_result_t toml = toml_parse_file_ex(kb_path);

    if(!toml.ok) {
        RD_LOG_FAIL("%s", toml.errmsg);
        return NULL;
    }

    kbfile = _rd_kbfile_create();
    kbfile->name = rd_strdup(kb);
    kbfile->toml = toml;
    kbfile->root = rd_i_kb_from_datum(&kbfile->toml.toptab);

    vect_push(&ctx->kb->files, kbfile); // avoid recursion
    _rd_kb_load_dependencies(kbfile->root, ctx);

    RD_LOG_INFO("loading KB '%s'", kb);
    _rd_kb_load_compound(kbfile->root, ctx, "unions");
    _rd_kb_load_compound(kbfile->root, ctx, "structs");
    _rd_kb_load_enums(kbfile->root, ctx);
    _rd_kb_load_functions(kbfile->root, ctx);
    _rd_kb_load_ordinals(kbfile->root, ctx);

    return kbfile->root;
}
