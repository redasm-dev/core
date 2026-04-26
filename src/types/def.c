#include "def.h"
#include "core/context.h"
#include "support/containers.h"
#include "support/error.h"
#include "support/logging.h"
#include "types/type.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define RD_PRIMITIVE(n, s)                                                     \
    {                                                                          \
        .name = n,                                                             \
        .kind = RD_TKIND_PRIM,                                                 \
        .size = s,                                                             \
    }

static RDTypeDef t_prim_u8 = RD_PRIMITIVE("u8", sizeof(u8));
static RDTypeDef t_prim_u16 = RD_PRIMITIVE("u16", sizeof(u16));
static RDTypeDef t_prim_u32 = RD_PRIMITIVE("u32", sizeof(u32));
static RDTypeDef t_prim_u64 = RD_PRIMITIVE("u64", sizeof(u64));
static RDTypeDef t_prim_i8 = RD_PRIMITIVE("i8", sizeof(i8));
static RDTypeDef t_prim_i16 = RD_PRIMITIVE("i16", sizeof(i16));
static RDTypeDef t_prim_i32 = RD_PRIMITIVE("i32", sizeof(i32));
static RDTypeDef t_prim_i64 = RD_PRIMITIVE("i64", sizeof(i64));
static RDTypeDef t_prim_char = RD_PRIMITIVE("char", sizeof(char));
static RDTypeDef t_prim_char16 = RD_PRIMITIVE("char16", sizeof(u16));

static bool _rd_typedef_enum_in_range(const char* type, i64 val) {
    if(!strcmp(type, "u8")) return val >= 0 && val <= UINT8_MAX;
    if(!strcmp(type, "u16")) return val >= 0 && val <= UINT16_MAX;
    if(!strcmp(type, "u32")) return val >= 0 && val <= UINT32_MAX;
    if(!strcmp(type, "u64")) return val >= 0; // i64 can't hold full u64 range
    if(!strcmp(type, "i8")) return val >= INT8_MIN && val <= INT8_MAX;
    if(!strcmp(type, "i16")) return val >= INT16_MIN && val <= INT16_MAX;
    if(!strcmp(type, "i32")) return val >= INT32_MIN && val <= INT32_MAX;
    if(!strcmp(type, "i64")) return true;
    return false;
}

static RDTypeDef* _rd_typedef_create(RDContext* ctx, const char* name,
                                     RDTypeKind kind) {
    if(!name) return NULL;

    RDTypeDef* self = malloc(sizeof(*self));

    *self = (RDTypeDef){
        .name = rd_i_strpool_intern(&ctx->strings, name),
        .kind = kind,
    };

    return self;
}

RDTypeDef* rd_typedef_create_func(const char* name, RDContext* ctx) {
    return _rd_typedef_create(ctx, name, RD_TKIND_FUNC);
}

RDTypeDef* rd_typedef_create_struct(const char* name, RDContext* ctx) {
    return _rd_typedef_create(ctx, name, RD_TKIND_STRUCT);
}

RDTypeDef* rd_typedef_create_union(const char* name, RDContext* ctx) {
    return _rd_typedef_create(ctx, name, RD_TKIND_UNION);
}

RDTypeDef* rd_typedef_create_enum(const char* name, const char* type,
                                  RDContext* ctx) {
    RDTypeDef* self = _rd_typedef_create(ctx, name, RD_TKIND_ENUM);
    self->enum_.base_type = rd_i_strpool_intern(&ctx->strings, type);
    return self;
}

bool rd_typedef_add_member(RDTypeDef* self, const char* type, const char* name,
                           usize n, usize flags, RDContext* ctx) {
    RDParam m = {
        .type =
            {
                .name = rd_i_strpool_intern(&ctx->strings, type),
                .flags = flags,
                .count = n,
            },
        .name = rd_i_strpool_intern(&ctx->strings, name),
    };

    if(rd_i_typedef_is_compound(self)) {
        vect_push(&self->compound_, m);
        return true;
    }

    LOG_FAIL("cannot add members to '%s'", self->name);
    return false;
}

bool rd_typedef_add_enumval(RDTypeDef* self, const char* name, i64 value,
                            RDContext* ctx) {
    if(self->kind != RD_TKIND_ENUM) {
        LOG_FAIL("cannot add enumval to '%s'", self->name);
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
                        usize n, usize flags, RDContext* ctx) {
    if(self->kind != RD_TKIND_FUNC) {
        LOG_FAIL("cannot add argument to '%s'", self->name);
        return false;
    }

    vect_push(&self->func_.args,
              (RDParam){
                  .type =
                      {
                          .name = rd_i_strpool_intern(&ctx->strings, type),
                          .flags = flags,
                          .count = n,
                      },
                  .name = rd_i_strpool_intern(&ctx->strings, name),
              });

    return true;
}

bool rd_typedef_set_ret(RDTypeDef* self, const char* type, usize n, usize flags,
                        RDContext* ctx) {
    if(self->kind != RD_TKIND_FUNC) {
        LOG_FAIL("cannot set return type to '%s'", self->name);
        return false;
    }

    self->func_.ret = (RDType){
        .name = rd_i_strpool_intern(&ctx->strings, type),
        .flags = flags,
        .count = n,
    };

    return true;
}

RDTypeDef* rd_i_typedef_find(const RDContext* ctx, const char* name,
                             bool required) {
    RDTypeDef** it;
    vect_each(it, &ctx->types) {
        if(strcmp((*it)->name, name) == 0) return *it;
    }

    panic_if(required, "type '%s' not found in registry", name);
    return NULL;
}

bool rd_typedef_register(RDTypeDef* self, RDContext* ctx) {
    if(rd_i_typedef_find(ctx, self->name, false)) {
        LOG_FAIL("'%s' already registered", self->name);
        return false;
    }

    if(rd_i_typedef_is_compound(self)) {
        if(vect_is_empty(&self->compound_)) {
            LOG_FAIL("at least one member required for '%s'", self->name);
            return false;
        }

        RDParam* m;
        vect_each(m, &self->compound_) {
            const RDTypeDef* tdef = rd_i_typedef_find(ctx, m->type.name, false);
            if(!tdef) {
                LOG_FAIL("type '%s' not found for '%s.%s'", m->type.name,
                         self->name, m->name);
                return false;
            }
        }
    }
    else if(self->kind == RD_TKIND_ENUM) {
        const RDTypeDef* tdef =
            rd_i_typedef_find(ctx, self->enum_.base_type, false);
        if(!tdef) {
            LOG_FAIL("type '%s' not found in enum '%s'", self->enum_.base_type,
                     self->name);
            return false;
        }

        if(tdef->kind != RD_TKIND_PRIM) {
            LOG_FAIL("type '%s' is not primitive", self->enum_.base_type);
            return false;
        }

        RDEnumCase* c1;
        vect_each(c1, &self->enum_) {
            if(!_rd_typedef_enum_in_range(self->enum_.base_type, c1->value)) {
                LOG_FAIL("value '%lld' out of range for type '%s' in enum '%s'",
                         c1->value, self->enum_.base_type, self->name);
                return false;
            }

            RDEnumCase* c2;
            vect_each(c2, &self->enum_) {
                if(c1 != c2 && !strcmp(c1->name, c2->name)) {
                    LOG_FAIL("duplicate case '%s' in enum '%s'", c1->name,
                             self->enum_.base_type);
                    return false;
                }
            }
        }
    }
    else if(self->kind == RD_TKIND_FUNC) {
        for(usize i = 0; i < vect_length(&self->func_.args); i++) {
            RDParam* arg1 = vect_at(&self->func_.args, i);

            if(!arg1->name) {
                LOG_FAIL("function '%s': argument %d has no name", self->name,
                         i + 1);
                return false;
            }

            if(rd_i_is_void(&arg1->type)) {
                LOG_FAIL("function '%s': argument %d '%s' cannot be void",
                         self->name, i + 1, arg1->name);
                return false;
            }

            for(usize j = i + 1; j < vect_length(&self->func_.args); j++) {
                RDParam* arg2 = vect_at(&self->func_.args, j);

                if(!strcmp(arg1->name, arg2->name)) {
                    LOG_FAIL(
                        "function '%s': argument %d has duplicate name '%s'",
                        self->name, i + 1, arg1->name);
                    return false;
                }
            }
        }
    }
    else if(self->kind != RD_TKIND_PRIM)
        unreachable();

    rd_i_db_set_type_def(ctx, self);
    vect_push(&ctx->types, self);
    LOG_DEBUG("definition '%s' registered", self->name);
    return true;
}

void rd_i_typedef_destroy(RDTypeDef* self) {
    if(rd_i_typedef_is_compound(self))
        vect_destroy(&self->compound_);
    else if(self->kind == RD_TKIND_ENUM) {
        vect_destroy(&self->enum_);
    }
    else if(self->kind == RD_TKIND_PRIM)
        return;

    free(self);
}

void rd_i_typedef_resolve_size(const RDContext* ctx, RDTypeDef* tdef) {
    if(tdef->size > 0) return;

    if(tdef->kind == RD_TKIND_STRUCT) {
        RDParam* m;
        vect_each(m, &tdef->compound_) {
            usize member_sz =
                rd_i_size_of(ctx, m->type.name, m->type.count, m->type.flags);

            panic_if(!member_sz,
                     "struct member '%s.%s' has unresolved size, ensure "
                     "'%s' is registered before '%s'",
                     tdef->name, m->name, m->type.name, tdef->name);

            tdef->size += member_sz;
        }
    }
    else if(tdef->kind == RD_TKIND_UNION) {
        RDParam* m;
        vect_each(m, &tdef->compound_) {
            usize member_sz =
                rd_i_size_of(ctx, m->type.name, m->type.count, m->type.flags);

            panic_if(!member_sz,
                     "union member '%s.%s' has unresolved size, ensure "
                     "'%s' is registered before '%s'",
                     tdef->name, m->name, m->type.name, tdef->name);

            if(member_sz > tdef->size) tdef->size = member_sz;
        }
    }
    else if(tdef->kind == RD_TKIND_ENUM) {
        tdef->size = rd_i_size_of(ctx, tdef->enum_.base_type, 0, RD_TYPE_NONE);
    }
    else
        panic("unsupported type kind '%lld'", tdef->kind);
}

void rd_i_register_primitives(RDContext* ctx) {
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
