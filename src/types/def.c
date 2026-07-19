#include "def.h"
#include "core/context.h"
#include "support/containers.h"
#include "support/error.h"
#include <redasm/support/logging.h>
#include <stdint.h>
#include <string.h>

#define RD_PRIMITIVE(n, s)                                                     \
    {                                                                          \
        .name = n,                                                             \
        .kind = RD_TKIND_PRIM,                                                 \
        .size = s,                                                             \
    }

static RDTypeDef t_prim_bool = RD_PRIMITIVE("bool", sizeof(bool));
static RDTypeDef t_prim_u8 = RD_PRIMITIVE("u8", sizeof(u8));
static RDTypeDef t_prim_u16 = RD_PRIMITIVE("u16", sizeof(u16));
static RDTypeDef t_prim_u32 = RD_PRIMITIVE("u32", sizeof(u32));
static RDTypeDef t_prim_u64 = RD_PRIMITIVE("u64", sizeof(u64));
static RDTypeDef t_prim_i8 = RD_PRIMITIVE("i8", sizeof(i8));
static RDTypeDef t_prim_i16 = RD_PRIMITIVE("i16", sizeof(i16));
static RDTypeDef t_prim_i32 = RD_PRIMITIVE("i32", sizeof(i32));
static RDTypeDef t_prim_i64 = RD_PRIMITIVE("i64", sizeof(i64));
static RDTypeDef t_prim_char = RD_PRIMITIVE("char", sizeof(i8));
static RDTypeDef t_prim_char16 = RD_PRIMITIVE("char16", sizeof(i16));

static bool _rd_typedef_enum_in_range(const RDTypeDef* tdef, i64 val) {
    if(!strcmp(tdef->name, "u8")) return val >= 0 && val <= UINT8_MAX;
    if(!strcmp(tdef->name, "u16")) return val >= 0 && val <= UINT16_MAX;
    if(!strcmp(tdef->name, "u32")) return val >= 0 && val <= UINT32_MAX;
    if(!strcmp(tdef->name, "u64"))
        return val >= 0; // i64 can't hold full u64 range
    if(!strcmp(tdef->name, "i8")) return val >= INT8_MIN && val <= INT8_MAX;
    if(!strcmp(tdef->name, "i16")) return val >= INT16_MIN && val <= INT16_MAX;
    if(!strcmp(tdef->name, "i32")) return val >= INT32_MIN && val <= INT32_MAX;
    if(!strcmp(tdef->name, "i64")) return true;
    return false;
}

static const char* _rd_typedef_kind_str(RDTypeKind kind) {
    switch(kind) {
        case RD_TKIND_PRIM: return "primitive";
        case RD_TKIND_STRUCT: return "struct";
        case RD_TKIND_UNION: return "union";
        case RD_TKIND_ENUM: return "enum";
        case RD_TKIND_FUNC: return "function";
        default: break;
    }

    return "???";
}

static RDTypeDef* _rd_typedef_create(const char* name, RDTypeKind kind,
                                     RDContext* ctx) {
    assert(kind < RD_TKIND_COUNT);

    if(!name) return NULL;

    RDTypeDef* self = rd_alloc(sizeof(*self));

    *self = (RDTypeDef){
        .name = rd_i_strpool_intern(&ctx->strings, name),
        .kind = kind,
    };

    return self;
}

static void _rd_typedef_resolve_size(const RDContext* ctx, RDTypeDef* tdef) {
    if(tdef->size > 0) return;

    switch(tdef->kind) {
        case RD_TKIND_STRUCT: {
            RDParam* m;
            vect_each(m, &tdef->compound_) {
                usize member_sz = rd_type_size(&m->type, ctx);
                m->field_offset = tdef->size;
                tdef->size += member_sz;
            }

            break;
        }

        case RD_TKIND_UNION: {
            RDParam* m;
            vect_each(m, &tdef->compound_) {
                usize member_sz = rd_type_size(&m->type, ctx);
                m->field_offset = 0; // union members shares the same offset
                if(member_sz > tdef->size) tdef->size = member_sz;
            }

            break;
        }

        case RD_TKIND_ENUM: tdef->size = tdef->enum_.base_type->size; break;
        case RD_TKIND_FUNC: tdef->size = rd_get_code_ptr_size(ctx); break;
        case RD_TKIND_PRIM: break;
        default: panic("unsupported type kind '%lld'", tdef->kind); break;
    }
}

RDTypeDef* rd_typedef_create_func(const char* name, RDContext* ctx) {
    return _rd_typedef_create(name, RD_TKIND_FUNC, ctx);
}

RDTypeDef* rd_typedef_create_struct(const char* name, RDContext* ctx) {
    return _rd_typedef_create(name, RD_TKIND_STRUCT, ctx);
}

RDTypeDef* rd_typedef_create_union(const char* name, RDContext* ctx) {
    return _rd_typedef_create(name, RD_TKIND_UNION, ctx);
}

RDTypeDef* rd_typedef_create_enum(const char* name, const char* type,
                                  RDContext* ctx) {
    if(!type) return NULL;

    const RDTypeDef* tdef = rd_i_typedef_find(ctx, type);

    if(!tdef) {
        RD_LOG_FAIL("enum base-type '%s' not found", type);
        return NULL;
    }

    if(tdef->kind != RD_TKIND_PRIM) {
        RD_LOG_FAIL("enum base-type '%s' is not primitive", type);
        return NULL;
    }

    RDTypeDef* self = _rd_typedef_create(name, RD_TKIND_ENUM, ctx);
    self->enum_.base_type = tdef;
    return self;
}

RDParamSlice rd_typedef_get_members(const RDTypeDef* self) {
    if(rd_i_typedef_is_compound(self))
        return vect_to_slice(RDParamSlice, &self->compound_);

    return (RDParamSlice){0};
}

RDParamSlice rd_typedef_get_args(const RDTypeDef* self) {
    if(self->kind == RD_TKIND_FUNC)
        return vect_to_slice(RDParamSlice, &self->func_.args);

    return (RDParamSlice){0};
}

RDType rd_typedef_get_ret(const RDTypeDef* self) {
    if(self->kind == RD_TKIND_FUNC) return self->func_.ret;
    return (RDType){0};
}

const RDTypeDef* rd_typedef_get_base_type(const RDTypeDef* self) {
    if(self->kind == RD_TKIND_ENUM) return self->enum_.base_type;
    return NULL;
}

bool rd_typedef_add_member(RDTypeDef* self, const char* type, const char* name,
                           usize n, RDTypeModifier mod, RDContext* ctx) {
    if(!rd_i_typedef_is_compound(self)) {
        RD_LOG_FAIL("cannot add members to '%s'", self->name);
        return false;
    }

    if(!type) {
        RD_LOG_FAIL("member type is NULL for '%s' type", self->name);
        return false;
    }

    if(!name) {
        RD_LOG_FAIL("member name is NULL for '%s' type", self->name);
        return false;
    }

    RDParam m = {0};
    if(!rd_type_init(&m.type, type, n, mod, ctx)) return false;
    m.name = rd_i_strpool_intern(&ctx->strings, name);
    vect_push(&self->compound_, m);
    return true;
}

bool rd_typedef_add_enumval(RDTypeDef* self, const char* name, i64 value,
                            RDContext* ctx) {
    if(self->kind != RD_TKIND_ENUM) {
        RD_LOG_FAIL("cannot add enumval to '%s'", self->name);
        return false;
    }

    vect_push(&self->enum_,
              (RDEnumCase){
                  .name = rd_i_strpool_intern(&ctx->strings, name),
                  .value = value,
              });

    return true;
}

bool rd_typedef_add_arg(RDTypeDef* self, const char* type, const char* name,
                        usize n, RDTypeModifier mod, RDContext* ctx) {
    if(self->kind != RD_TKIND_FUNC) {
        RD_LOG_FAIL("cannot add argument to '%s'", self->name);
        return false;
    }

    RDType t;
    if(!rd_type_init(&t, type, n, mod, ctx)) return false;

    vect_push(&self->func_.args,
              (RDParam){
                  .type = t,
                  .name = rd_i_strpool_intern(&ctx->strings, name),
              });

    return true;
}

bool rd_typedef_set_ret(RDTypeDef* self, const char* type, usize n,
                        RDTypeModifier mod, RDContext* ctx) {
    if(self->kind != RD_TKIND_FUNC) {
        RD_LOG_FAIL("cannot set return type to '%s'", self->name);
        return false;
    }

    if(type) {
        if(!rd_type_init(&self->func_.ret, type, n, mod, ctx)) return false;
    }
    else
        rd_type_init_void(&self->func_.ret);

    return true;
}

bool rd_typedef_set_noret(RDTypeDef* self, bool b) {
    if(self->kind != RD_TKIND_FUNC) {
        RD_LOG_FAIL("cannot noret to '%s'", self->name);
        return false;
    }

    self->func_.is_noret = b;
    return true;
}

RDTypeDef* rd_i_typedef_find(const RDContext* ctx, const char* name) {
    RDTypeDef** it;
    vect_each(it, &ctx->typedefs) {
        if(!strcmp((*it)->name, name)) return *it;
    }

    return NULL;
}

bool rd_typedef_register(RDTypeDef* self, RDContext* ctx) {
    if(rd_i_typedef_find(ctx, self->name)) {
        RD_LOG_FAIL("'%s' already registered", self->name);
        goto fail;
    }

    if(rd_i_typedef_is_compound(self)) {
        if(vect_is_empty(&self->compound_)) {
            RD_LOG_FAIL("at least one member required for '%s'", self->name);
            goto fail;
        }
    }
    else if(self->kind == RD_TKIND_ENUM) {
        RDEnumCase* c1;
        vect_each(c1, &self->enum_) {
            if(!_rd_typedef_enum_in_range(self->enum_.base_type, c1->value)) {
                RD_LOG_FAIL(
                    "value '%lld' out of range for type '%s' in enum '%s'",
                    c1->value, self->enum_.base_type, self->name);
                goto fail;
            }

            RDEnumCase* c2;
            vect_each(c2, &self->enum_) {
                if(c1 != c2 && !strcmp(c1->name, c2->name)) {
                    RD_LOG_FAIL("duplicate case '%s' in enum '%s'", c1->name,
                                self->enum_.base_type);
                    goto fail;
                }
            }
        }
    }
    else if(self->kind == RD_TKIND_FUNC) {
        for(usize i = 0; i < vect_length(&self->func_.args); i++) {
            RDParam* arg1 = vect_at(&self->func_.args, i);

            if(!arg1->name) {
                RD_LOG_FAIL("function '%s': argument %d has no name",
                            self->name, i + 1);
                goto fail;
            }

            if(rd_type_is_void(&arg1->type)) {
                RD_LOG_FAIL("function '%s': argument %d '%s' cannot be void",
                            self->name, i + 1, arg1->name);
                goto fail;
            }

            for(usize j = i + 1; j < vect_length(&self->func_.args); j++) {
                RDParam* arg2 = vect_at(&self->func_.args, j);

                if(!strcmp(arg1->name, arg2->name)) {
                    RD_LOG_FAIL(
                        "function '%s': argument %d has duplicate name '%s'",
                        self->name, i + 1, arg1->name);
                    goto fail;
                }
            }
        }
    }
    else if(self->kind != RD_TKIND_PRIM)
        unreachable();

    if(self->kind == RD_TKIND_FUNC && self->func_.is_noret)
        rd_i_kb_add_noret(ctx, self->name);

    if(self->kind != RD_TKIND_PRIM) {
        rd_i_db_set_type_def(ctx, self); // don't save primitives in DB

        RD_LOG_INFO("%s definition '%s' registered",
                    _rd_typedef_kind_str(self->kind), self->name);
    }

    _rd_typedef_resolve_size(ctx, self);

    if(!self->size) {
        RD_LOG_FAIL("type '%s' has unresolved size", self->name);
        goto fail;
    }

    vect_push(&ctx->typedefs, self);
    return true;

fail:
    rd_typedef_destroy(self);
    return false;
}

const char* rd_typedef_name(const RDTypeDef* self) { return self->name; }
usize rd_typedef_size(const RDTypeDef* self) { return self->size; }

void rd_typedef_destroy(RDTypeDef* self) {
    if(rd_i_typedef_is_compound(self))
        vect_destroy(&self->compound_);
    else if(self->kind == RD_TKIND_FUNC) {
        vect_destroy(&self->func_.args);
    }
    else if(self->kind == RD_TKIND_ENUM) {
        vect_destroy(&self->enum_);
    }
    else if(self->kind == RD_TKIND_PRIM)
        return;

    rd_free(self);
}

bool rd_typedef_resolve_offset(RDContext* ctx, const RDTypeDef* tdef,
                               usize offset, const RDParam** m) {
    if(tdef->kind != RD_TKIND_STRUCT) return false;

    const RDParam* p;
    vect_each(p, &tdef->compound_) {
        usize span = rd_type_size(&p->type, ctx);

        if(offset >= p->field_offset && offset < p->field_offset + span) {
            if(m) *m = p;
            return true;
        }
    }

    return false;
}

void rd_i_register_primitives(RDContext* ctx) {
    rd_typedef_register(&t_prim_bool, ctx);
    rd_typedef_register(&t_prim_u8, ctx);
    rd_typedef_register(&t_prim_u16, ctx);
    rd_typedef_register(&t_prim_u32, ctx);
    rd_typedef_register(&t_prim_u64, ctx);
    rd_typedef_register(&t_prim_i8, ctx);
    rd_typedef_register(&t_prim_i16, ctx);
    rd_typedef_register(&t_prim_i32, ctx);
    rd_typedef_register(&t_prim_i64, ctx);
    rd_typedef_register(&t_prim_char, ctx);
    rd_typedef_register(&t_prim_char16, ctx);
}
