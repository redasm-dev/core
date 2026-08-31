#include "kb.h"
#include "core/context.h"
#include "core/state.h"
#include "kb/object.h"
#include "kb/schema.h"
#include "support/containers.h"
#include "support/error.h"
#include "support/tomlschema.h"
#include <errno.h>
#include <inttypes.h>
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

static bool _rd_kb_get_type(const RDKBObject* obj, RDType* t,
                            const RDContext* ctx) {
    *t = (RDType){.mod = RD_TYPE_NONE};

    i64 count = 0;
    const char* tname = rd_kbobject_get_str(obj, "type");
    assert(tname);

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

    if(t->mod == RD_TYPE_NONE) {
        if(is_mod_str_cptr)
            t->mod = RD_TYPE_CPTR;
        else if(is_mod_str_ptr)
            t->mod = RD_TYPE_PTR;
    }

    if(!t->def) t->def = rd_i_typedef_find(ctx, tname);

    if(!t->def) {
        RD_LOG_FAIL("cannot find type '%s'", tname);
        return false;
    }

    return true;
}

// push the current manifest (if any):
// - NULL 'callconv' is valid and it means "no specified"
static bool _rd_kb_push_manifest(const RDKBObject* root, RDContext* ctx) {
    const RDKBObject* manifest = rd_kbobject_get_table(root, "manifest");
    if(manifest && !rd_i_kb_validate_manifest(manifest)) return false;

    // Resolve and push first: dependencies are includes and inherit this.
    const char* callconv = rd_kbobject_get_str(manifest, "callconv");

    if(!callconv && !vect_is_empty(&ctx->kb->curr_callconv))
        callconv = *vect_last(&ctx->kb->curr_callconv);

    vect_push(&ctx->kb->curr_callconv, callconv);

    const RDKBObject* dependencies =
        rd_kbobject_get_array(manifest, "dependencies");

    if(dependencies) {
        const RDKBObject* dep;
        rd_kbobject_each(dep, dependencies) {
            const char* dep_path = rd_kbobject_to_str(dep);
            if(dep_path) rd_kb_load(ctx, dep_path);
        }
    }

    return true;
}

static void _rd_kb_pop_manifest(RDContext* ctx) {
    vect_pop_last(&ctx->kb->curr_callconv);
}

static void _rd_kb_load_compounds(const RDKBObject* types, RDContext* ctx,
                                  RDTypeKind kind) {
    const char* name;
    const RDKBObject* def;
    rd_kbobject_each_pair(name, def, types) {
        if(!rd_i_kb_validate_compound(def)) continue;

        RDTypeDef* tdef = NULL;

        if(kind == RD_TKIND_STRUCT)
            tdef = rd_typedef_create_struct(name, ctx);
        else if(kind == RD_TKIND_UNION)
            tdef = rd_typedef_create_union(name, ctx);
        else
            unreachable();

        const RDKBObject* members = rd_kbobject_get_array(def, "members");
        assert(members);

        const RDKBObject* m;
        rd_kbobject_each(m, members) {
            RDType t;
            if(!_rd_kb_get_type(m, &t, ctx)) {
                rd_typedef_destroy(tdef);
                return;
            }

            const char* param_name = rd_kbobject_get_str(m, "name");
            if(!param_name) {
                rd_typedef_destroy(tdef);
                return;
            }

            if(!rd_typedef_add_member(tdef, t.def->name, param_name, t.count,
                                      t.mod, ctx)) {
                rd_typedef_destroy(tdef);
                return;
            }
        }

        rd_typedef_register(tdef, ctx);
    }
}

static void _rd_kb_load_structs(const RDKBObject* types, RDContext* ctx) {
    _rd_kb_load_compounds(types, ctx, RD_TKIND_STRUCT);
}

static void _rd_kb_load_unions(const RDKBObject* types, RDContext* ctx) {
    _rd_kb_load_compounds(types, ctx, RD_TKIND_UNION);
}

static void _rd_kb_load_enums(const RDKBObject* enums, RDContext* ctx) {
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
                return;
            }

            if(!rd_typedef_add_enumval(tdef, m_name, m_value, ctx)) {
                rd_typedef_destroy(tdef);
                return;
            }
        }

        rd_typedef_register(tdef, ctx);
    }
}

static void _rd_kb_load_funcs(const RDKBObject* functions, RDContext* ctx,
                              RDTypeFlags flags) {
    const char* name;
    const RDKBObject* f;
    rd_kbobject_each_pair(name, f, functions) {
        if(!rd_i_kb_validate_function(f)) continue;

        RDTypeDef* tdef = rd_typedef_create_func(name, ctx);
        rd_typedef_set_proto(tdef, flags & RD_TFLAGS_PROTOTYPE);

        bool is_noret = false;
        rd_kbobject_get_bool(f, "noret", &is_noret);
        rd_typedef_set_noret(tdef, is_noret);

        // try explicit 'callconv' or manifest provided one (if any)
        const char* callconv = rd_kbobject_get_str(f, "callconv");
        if(!callconv) callconv = *vect_last(&ctx->kb->curr_callconv);
        rd_typedef_set_callconv(tdef, callconv);

        const RDKBObject* ret = rd_kbobject_get(f, "ret");
        assert(ret);

        RDType ret_type;
        if(!_rd_kb_get_type(ret, &ret_type, ctx)) {
            rd_typedef_destroy(tdef);
            return;
        }

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
            if(!_rd_kb_get_type(a, &t, ctx)) {
                rd_typedef_destroy(tdef);
                return;
            }

            const char* arg_name = rd_kbobject_get_str(a, "name");
            if(!arg_name) {
                rd_typedef_destroy(tdef);
                return;
            }

            if(!rd_typedef_add_arg(tdef, t.def->name, arg_name, t.count, t.mod,
                                   ctx)) {
                rd_typedef_destroy(tdef);
                return;
            }
        }

        rd_typedef_register(tdef, ctx);
    }
}

static void _rd_kb_load_functions(const RDKBObject* functions, RDContext* ctx) {
    _rd_kb_load_funcs(functions, ctx, RD_TFLAGS_NONE);
}

static void _rd_kb_load_prototypes(const RDKBObject* functions,
                                   RDContext* ctx) {
    _rd_kb_load_funcs(functions, ctx, RD_TFLAGS_PROTOTYPE);
}

static void _rd_kb_load_symbols(const RDKBObject* symbols, RDContext* ctx) {
    const char* name;
    const RDKBObject* sym;
    rd_kbobject_each_pair(name, sym, symbols) {
        if(!rd_i_kb_validate_symbol(sym)) continue;

        i64 addr_v;
        rd_kbobject_get_int(sym, "address", &addr_v);

        RDAddress address = (RDAddress)addr_v;

        bool is_func;
        bool has_func = rd_kbobject_get_bool(sym, "function", &is_func);
        const RDKBObject* type = rd_kbobject_get_table(sym, "type");
        const char* module = rd_kbobject_get_str(sym, "module");
        const char* external = rd_kbobject_get_str(sym, "external");

        if(has_func && type) {
            RD_LOG_FAIL("symbol '%s' @ %" PRIX64
                        ": 'function' and 'type' are mutually "
                        "exclusive, skipping entry",
                        name, address);
            continue;
        }

        rd_library_name(ctx, address, name);

        if(external) {
            if(!strcmp(external, "imported"))
                rd_set_external(ctx, address, module, RD_EXT_IMPORTED);
            else if(!strcmp(external, "exported"))
                rd_set_external(ctx, address, module, RD_EXT_EXPORTED);
        }

        if(has_func) {
            if(is_func) rd_set_function(ctx, address);
        }
        else if(type) {
            const char* type_name = rd_kbobject_get_str(type, "name");
            const char* type_mod = rd_kbobject_get_str(type, "mod");

            RDTypeModifier mod = RD_TYPE_NONE;

            if(type_mod) {
                if(!strcmp(type_mod, "ptr"))
                    mod = RD_TYPE_PTR;
                else if(!strcmp(type_mod, "cptr"))
                    mod = RD_TYPE_CPTR;
            }

            i64 type_count;
            bool has_count = rd_kbobject_get_int(type, "count", &type_count);

            rd_library_type(ctx, address, type_name,
                            has_count ? (usize)type_count : 0, mod);
        }
    }
}

static void _rd_kb_load_ordinals(const RDKBObject* ordinals, RDContext* ctx) {
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
}

static void _rd_kb_load_callconvs(const RDKBObject* callconvs, RDContext* ctx) {
    const char* cc_kb_name;
    const RDKBObject* cc_kb;
    rd_kbobject_each_pair(cc_kb_name, cc_kb, callconvs) {
        if(!rd_i_kb_validate_callconv(cc_kb)) continue;

        if(rd_i_callconv_find(ctx, cc_kb_name)) {
            RD_LOG_WARN("calling convention '%s' already registered",
                        cc_kb_name);
            continue;
        }

        RDCallConv* cc = rd_i_callconv_create(cc_kb_name, ctx);
        const RDKBObject* arg_regs = rd_kbobject_get_array(cc_kb, "arg_regs");

        if(arg_regs) {
            const RDKBObject* arg_reg;
            rd_kbobject_each(arg_reg, arg_regs) {
                const char* regname = rd_kbobject_to_str(arg_reg);
                RDQueryReg q = {.kind = RD_QUERY_REG_BY_NAME, .name = regname};

                if(!rd_query_reg(ctx, &q)) {
                    RD_LOG_FAIL("calling convention '%s': unknown register "
                                "'%s' in 'arg_regs'",
                                cc_kb_name, regname);
                    goto discard;
                }

                vect_push(&cc->arg_regs,
                          rd_i_strpool_intern(&ctx->strings, regname));
            }
        }

        const char* arg_order = rd_kbobject_get_str(cc_kb, "arg_order");
        cc->arg_order =
            !strcmp(arg_order, "ltr") ? RD_ARGORDER_LTR : RD_ARGORDER_RTL;

        const char* stack_cleanup = rd_kbobject_get_str(cc_kb, "stack_cleanup");
        cc->stack_cleanup = !strcmp(stack_cleanup, "caller")
                                ? RD_STACK_CLEANUP_CALLER
                                : RD_STACK_CLEANUP_CALLEE;

        i64 shadow_space;
        if(rd_kbobject_get_int(cc_kb, "shadow_space", &shadow_space))
            cc->shadow_space = (usize)shadow_space;

        vect_push(&ctx->callconvs, cc);
        continue;

    discard:
        rd_i_callconv_destroy(cc);
    }
}

typedef struct RDKBCategory {
    const char* name;
    void (*load)(const RDKBObject*, RDContext*);
} RDKBCategory;

static const RDKBCategory KB_CATEGORIES[] = {
    {"unions", _rd_kb_load_unions},
    {"structs", _rd_kb_load_structs},
    {"enums", _rd_kb_load_enums},
    {"callconvs", _rd_kb_load_callconvs},
    {"prototypes", _rd_kb_load_prototypes},
    {"functions", _rd_kb_load_functions},
    {"symbols", _rd_kb_load_symbols},
    {"ordinals", _rd_kb_load_ordinals},
    {NULL, NULL},
};

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
    vect_destroy(&self->curr_callconv);
    vect_destroy(&self->files);
    rd_free(self);
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
    RD_LOG_INFO("loading KB '%s'", kb);

    if(_rd_kb_push_manifest(kbfile->root, ctx)) {
        const char* cat;
        const RDKBObject* table;
        rd_kbobject_each_pair(cat, table, kbfile->root) {
            const RDKBCategory* c = KB_CATEGORIES;

            while(c->name) {
                if(!strcmp(c->name, cat)) {
                    c->load(table, ctx);
                    break;
                }

                c++;
            }
        }

        _rd_kb_pop_manifest(ctx);
    }

    return kbfile->root;
}
