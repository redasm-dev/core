#include "type.h"
#include "core/context.h"
#include "io/flagsbuffer.h"
#include "support/error.h"
#include "types/def.h"
#include <inttypes.h>
#include <redasm/support/logging.h>

typedef enum { RD_UNROLL_ROOT, RD_UNROLL_FIELD, RD_UNROLL_ITEM } RDUnrollKind;

static void _rd_unroll_type(RDContext* ctx, const RDSegmentFull* seg,
                            usize* idx, const RDType* t, RDUnrollKind kind,
                            bool coincident) {
    usize sz = rd_type_size(t, ctx);

    // strings are the one array kind that does NOT unroll: they render as
    // a single literal line, so their interior is tails, not element heads
    bool is_array =
        t->mod == RD_TYPE_NONE && t->count > 0 && !rd_type_is_string(t);

    bool is_compound = !t->count && rd_i_type_check_struct(t);
    bool recurse = is_array || is_compound;
    usize tailsz = recurse ? 1 : sz;

    if(kind == RD_UNROLL_ROOT)
        rd_i_flagsbuffer_set_type(seg->flags, *idx, tailsz);
    else if(coincident)
        rd_i_flagsbuffer_set_tail(seg->flags, *idx + 1, tailsz - 1);
    else if(kind == RD_UNROLL_FIELD)
        rd_i_flagsbuffer_set_field(seg->flags, *idx, tailsz);
    else if(kind == RD_UNROLL_ITEM)
        rd_i_flagsbuffer_set_item(seg->flags, *idx, tailsz);
    else
        unreachable();

    if(is_array) {
        RDType elem = *t;
        elem.count = 0;

        for(usize i = 0; i < t->count; i++)
            _rd_unroll_type(ctx, seg, idx, &elem, RD_UNROLL_ITEM, i == 0);
    }
    else if(is_compound) {
        const RDParam* m;
        usize i = 0;
        vect_each(m, &t->def->compound_) {
            _rd_unroll_type(ctx, seg, idx, &m->type, RD_UNROLL_FIELD, i == 0);
            i++;
        }
    }
    else
        *idx += sz;
}

static bool _rd_type_resolve(RDContext* ctx, const RDType* type, usize offset,
                             usize depth, RDResolveResultVect* out) {
    // array: the child is items[offset / item_size]
    if(type->count > 0) {
        RDType item_type = *type;
        item_type.count = 0;

        usize item_size = rd_type_size(&item_type, ctx);
        usize item_idx = offset / item_size;

        if(item_idx >= type->count) {
            RD_LOG_FAIL("offset %zu out of bounds for '%s[%zu]'", offset,
                        type->def->name, type->count);
            return false;
        }

        usize rel = offset - (item_idx * item_size);

        RDResolveResult r = {
            .field = {.type = item_type, .name = NULL},
            .depth = depth,
            .item_idx = {.has_value = true, .value = item_idx},
            .at_offset = (rel == 0),
        };

        vect_push(out, r);

        if(rel == 0 && !rd_i_type_has_more(&item_type)) return true;
        return _rd_type_resolve(ctx, &item_type, rel, depth + 1, out);
    }

    // pointer: opaque
    if(type->mod != RD_TYPE_NONE) {
        if(offset != 0) {
            RD_LOG_FAIL("offset %zu inside pointer-typed field", offset);
            return false;
        }

        RDResolveResult r = {
            .field = {.type = *type, .name = NULL},
            .depth = depth,
            .at_offset = true,
        };

        vect_push(out, r);
        return true;
    }

    const RDTypeDef* tdef_struct = rd_i_type_check_struct(type);

    // solid leaf (primitive, enum, union)
    if(!tdef_struct) {
        if(offset != 0) {
            RD_LOG_FAIL("offset %zu inside opaque type '%s'", offset,
                        type->def->name);
            return false;
        }

        RDResolveResult r = {
            .field = {.type = *type, .name = NULL},
            .depth = depth,
            .at_offset = true,
        };

        vect_push(out, r);
        return true;
    }

    // struct: the child is the member whose span covers 'offset'
    const RDParam* m;
    if(!rd_typedef_resolve_offset(ctx, tdef_struct, offset, &m)) {
        RD_LOG_FAIL("offset %zu not covered by '%s'", offset,
                    tdef_struct->name);
        return false;
    }

    usize rel = offset - m->field_offset;

    RDResolveResult r = {
        .field = *m,
        .depth = depth,
        .at_offset = (rel == 0),
    };

    vect_push(out, r);

    if(rel == 0 && !rd_i_type_has_more(&m->type)) return true;
    return _rd_type_resolve(ctx, &m->type, rel, depth + 1, out);
}

usize rd_i_size_of(const RDContext* ctx, const char* name, usize n,
                   RDTypeModifier mod) {
    RDTypeDef* tdef = rd_i_typedef_find(ctx, name);

    if(!tdef) {
        RD_LOG_FAIL("cannot get the size of '%s', type not found", name);
        return 0;
    }

    usize sz;

    if(mod == RD_TYPE_CPTR)
        sz = rd_get_code_ptr_size(ctx);
    else if(mod == RD_TYPE_PTR)
        sz = rd_get_ptr_size(ctx);
    else
        sz = tdef->size;

    return n > 0 ? sz * n : sz;
}

bool rd_type_init(RDType* self, const char* name, usize n, RDTypeModifier mod,
                  RDContext* ctx) {
    if(!name || !self) return false;

    const RDTypeDef* tdef = rd_i_typedef_find(ctx, name);

    if(!tdef) {
        RD_LOG_FAIL("type '%s' not found in registry", name);
        return false;
    }

    // 'void' alone has no size, so an array of it is meaningless.
    // 'void*' does, an array of pointers is fine.
    if(tdef == rd_i_typedef_get_void() && n > 0 && mod == RD_TYPE_NONE) {
        RD_LOG_FAIL("'void' cannot have a count");
        return false;
    }

    self->def = tdef;
    self->count = n;
    self->mod = mod;
    return true;
}

void rd_type_init_void(RDType* self) {
    *self = (RDType){
        .def = rd_i_typedef_get_void(),
    };
}

bool rd_type_is_void(const RDType* t) {
    return t && t->def == rd_i_typedef_get_void() && t->mod == RD_TYPE_NONE &&
           t->count == 0;
}

const char* rd_integral_from_size(unsigned int size) {
    switch(size) {
        case sizeof(u8): return "u8";
        case sizeof(u16): return "u16";
        case sizeof(u32): return "u32";
        case sizeof(u64): return "u64";
        default: break;
    }

    return NULL;
}

const RDTypeDef* rd_integral_typedef_from_size(unsigned int size,
                                               const RDContext* ctx) {
    return rd_i_typedef_find(ctx, rd_integral_from_size(size));
}

const char* rd_i_type_to_str(const RDType* t, RDCharVect* buf) {
    assert(t->def);

    str_clear(buf);
    str_append(buf, t->def->name);

    if(t->count > 0) {
        str_push(buf, '[');
        str_append(buf, rd_i_to_dec((i64)t->count));
        str_push(buf, ']');
    }

    switch(t->mod) {
        case RD_TYPE_PTR:
        case RD_TYPE_CPTR: str_push(buf, '*'); break;

        default: break;
    }

    return buf->data;
}

const char* rd_i_type_path(RDContext* ctx, RDAddress address, RDCharVect* buf) {
    const RDSegmentFull* seg = rd_i_db_find_segment(ctx, address);
    if(!seg) return NULL;

    usize idx = rd_i_address2index(seg, address);

    if(!rd_flagsbuffer_has_type(seg->flags, idx) &&
       !rd_flagsbuffer_has_field(seg->flags, idx) &&
       !rd_flagsbuffer_has_item(seg->flags, idx))
        return NULL;

    RDAddress root_address = address;
    RDType root;
    if(!rd_i_db_get_root_type(ctx, &root_address, &root)) return NULL;

    str_clear(buf);
    str_append(buf, root.def->name);

    RDResolveResultVect chain = {0};

    if(rd_i_type_resolve_chain(ctx, &root, address - root_address, &chain)) {
        const RDResolveResult* r;

        vect_each(r, &chain) {
            if(r->field.name) {
                str_push(buf, '.');
                str_append(buf, r->field.name);
            }

            /*
             * Stop at the first entity that actually starts here: anything
             * deeper is a coincidence sharing the byte, not part of this
             * address's description.
             */
            if(r->at_offset) break;
        }
    }

    vect_destroy(&chain);
    return buf->data;
}

const char* rd_type_to_str(const RDType* self, RDContext* ctx) {
    return rd_i_type_to_str(self, &ctx->type_buf);
}

bool rd_i_set_type(RDContext* ctx, RDAddress address, const char* name, usize n,
                   RDTypeModifier mod, RDConfidence c) {
    if(!name || !(*name)) return false;

    const RDSegmentFull* seg = rd_i_db_find_segment(ctx, address);
    if(!seg) return false;

    RDType t;
    if(!rd_type_init(&t, name, n, mod, ctx)) {
        RD_LOG_FAIL("type initialization failed");
        return false;
    }

    usize sz = rd_type_size(&t, ctx);

    usize idx = rd_i_address2index(seg, address);
    usize startidx_exp = idx, endidx_exp = startidx_exp + sz;
    rd_i_expand_range(ctx, seg, &startidx_exp, &endidx_exp);

    // an AUTO/LIBRARY caller must not silently reinterpret code as data:
    // that's the classic misclassification.
    // A USER asking for it is a deliberate override.
    if(c < RD_CONFIDENCE_USER &&
       rd_i_flagsbuffer_has_code_n(seg->flags, startidx_exp,
                                   endidx_exp - startidx_exp)) {
        return false;
    }

    if(!rd_i_undefine_n(ctx, address, sz, c)) return false;

    rd_i_type_unroll(ctx, address, &t);
    rd_i_db_set_type(ctx, address, &t, c);
    rd_i_engine_mark_dirty(ctx);
    return true;
}

bool rd_i_get_type(RDContext* ctx, RDAddress address, RDTypeFull* t) {
    const RDSegmentFull* seg = rd_i_db_find_segment(ctx, address);
    if(!seg) return false;

    usize idx = rd_i_address2index(seg, address);
    if(!rd_flagsbuffer_has_type(seg->flags, idx)) return false;

    bool ok = rd_i_db_get_type(ctx, address, t);
    assert(ok && "cannot find type in database");
    return true;
}

bool rd_get_type(RDContext* ctx, RDAddress address, RDType* t) {
    RDTypeFull tf;
    if(rd_i_get_type(ctx, address, &tf)) {
        if(t) *t = tf.base;
        return true;
    }

    return false;
}

bool rd_auto_type(RDContext* ctx, RDAddress address, const char* name, usize n,
                  RDTypeModifier flags) {
    return rd_i_set_type(ctx, address, name, n, flags, RD_CONFIDENCE_AUTO);
}

bool rd_library_type(RDContext* ctx, RDAddress address, const char* name,
                     usize n, RDTypeModifier flags) {
    return rd_i_set_type(ctx, address, name, n, flags, RD_CONFIDENCE_LIBRARY);
}

bool rd_user_type(RDContext* ctx, RDAddress address, const char* name, usize n,
                  RDTypeModifier flags) {
    return rd_i_set_type(ctx, address, name, n, flags, RD_CONFIDENCE_USER);
}

usize rd_type_size(const RDType* self, const RDContext* ctx) {
    if(rd_type_is_void(self)) return 0;

    usize sz = rd_i_size_of(ctx, self->def->name, self->count, self->mod);
    panic_if(!sz, "type '%s' has unresolved size", self->def->name);
    return sz;
}

bool rd_type_equals(const RDType* self, const RDType* t) {
    if(!self || !t) return false;
    if(self->count != t->count || self->mod != t->mod) return false;
    return self->def == t->def;
}

void rd_i_type_unroll(RDContext* ctx, RDAddress address, const RDType* t) {
    assert(t);

    const RDSegmentFull* seg = rd_i_db_find_segment(ctx, address);
    assert(seg);

    usize idx = rd_i_address2index(seg, address);
    _rd_unroll_type(ctx, seg, &idx, t, RD_UNROLL_ROOT, true);
}

const RDTypeDef* rd_i_type_check_struct(const RDType* t) {
    if(t->mod != RD_TYPE_NONE) return NULL;
    return t->def->kind == RD_TKIND_STRUCT ? t->def : NULL;
}

bool rd_i_type_resolve_chain(RDContext* ctx, const RDType* root, usize offset,
                             RDResolveResultVect* out) {
    if(!root || !out) return false;

    vect_clear(out);
    if(!_rd_type_resolve(ctx, root, offset, 0, out)) return false;

    return !vect_is_empty(out);
}

bool rd_i_type_has_more(const RDType* t) {
    if(t->mod != RD_TYPE_NONE) return false; // pointers are opaque
    if(rd_type_is_string(t)) return false;   // strings are one item
    if(t->count > 0) return true;            // array: elements inside
    return rd_i_type_check_struct(t);        // struct: elements inside
}

bool rd_type_is_string(const RDType* t) {
    if(!t->count || !t->def) return false;
    if(t->mod != RD_TYPE_NONE) return false;
    return !strcmp(t->def->name, "char") || !strcmp(t->def->name, "char16");
}

RDResolveResultSlice rd_type_resolve_chain(RDContext* ctx, const RDType* root,
                                           usize offset) {
    if(!rd_i_type_resolve_chain(ctx, root, offset, &ctx->resolve_buf))
        return (RDResolveResultSlice){0};

    return vect_to_slice(RDResolveResultSlice, &ctx->resolve_buf);
}
