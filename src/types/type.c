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

/*
 * One step of the resolution walk.
 *
 * Each step locates the child containing `offset` and computes
 * rel = offset - child_start.
 * Exactly two situations exist:
 *
 *   rel != 0  the offset is INSIDE the child.
 *             The child cannot be the answer (interiors are never entities),
 *             so descent is MANDATORY: min_depth is not consulted on this path.
 *
 *   rel == 0  the offset is on the child's edge: the child IS a valid
 *             answer, but a deeper one may share the byte (coincidence).
 *             Stop here if the walk is deep enough (depth >= min_depth),
 *             or unconditionally if the child is solid: declining a
 *             solid child would cross a wall into nothing, fabricating
 *             a depth that was never reached (the "phantom" bug).
 *
 * out_r->depth is a check of what actually crossed; the wrapper
 * zeroed it.
 * On success with depth < min_depth the caller learns the
 * schema ended before the requested floor, that is the honest
 * exhaustion signal the renderer's chain walk relies on.
 *
 * The terminal branches (pointer / solid leaf) carry no min_depth check
 * on purpose: they are only ever entered through a mandatory rel != 0
 * descent (the offset proved structure exists) or as the top-level call
 * itself: in both cases "the thing at its own edge" is the only answer
 * there is, at whatever depth the odometer honestly shows.
 */
static bool _rd_type_resolve(RDContext* ctx, const RDType* type, usize offset,
                             usize min_depth, RDResolveResult* out_r) {

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

        out_r->item_idx.has_value = true;
        out_r->item_idx.value = item_idx;

        usize rel = offset - (item_idx * item_size);

        if(rel == 0 &&
           (out_r->depth >= min_depth || !rd_i_type_has_more(&item_type))) {
            out_r->field = (RDParam){.type = item_type, .name = NULL};
            return true;
        }

        out_r->depth++;
        return _rd_type_resolve(ctx, &item_type, rel, min_depth, out_r);
    }

    // pointer: opaque
    if(type->mod != RD_TYPE_NONE) {
        if(offset != 0) {
            RD_LOG_FAIL("offset %zu inside pointer-typed field", offset);
            return false;
        }

        out_r->field = (RDParam){.type = *type, .name = NULL};
        return true;
    }

    const RDTypeDef* tdef_struct = rd_i_type_check_struct(type);

    // solid leaf (primitve, enum, union)
    if(!tdef_struct) {
        if(offset != 0) {
            RD_LOG_FAIL("offset %zu inside opaque type '%s'", offset,
                        type->def->name);
            return false;
        }

        out_r->field = (RDParam){.type = *type, .name = NULL};
        return true;
    }

    // struct: the child is the member whose span overs 'offset'
    const RDParam* m;
    if(!rd_typedef_resolve_offset(ctx, tdef_struct, offset, &m)) {
        RD_LOG_FAIL("offset %zu not covered by '%s'", offset,
                    tdef_struct->name);
        return false;
    }

    usize rel = offset - m->field_offset;

    if(rel == 0 &&
       (out_r->depth >= min_depth || !rd_i_type_has_more(&m->type))) {
        out_r->field = *m;
        return true;
    }

    out_r->depth++;
    return _rd_type_resolve(ctx, &m->type, rel, min_depth, out_r);
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

    self->def = tdef;
    self->count = n;
    self->mod = mod;
    return true;
}

void rd_type_init_void(RDType* self) { *self = (RDType){0}; }

const char* rd_integral_from_size(unsigned int size) {
    switch(size) {
        case sizeof(u8): return "u8";
        case sizeof(u16): return "u16";
        case sizeof(u32): return "u32";
        case sizeof(u64): return "u64";
        default: break;
    }

    panic("integral type not found for size: %u", size);
}

const RDTypeDef* rd_integral_typedef_from_size(unsigned int size,
                                               const RDContext* ctx) {
    return rd_i_typedef_find(ctx, rd_integral_from_size(size));
}

const char* rd_i_type_to_str(const RDType* t, RDCharVect* buf) {
    if(rd_type_is_void(t)) return "void";

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

bool rd_i_set_type(RDContext* ctx, RDAddress address, const char* name, usize n,
                   RDTypeModifier mod, RDConfidence c) {
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
    rd_i_flagsbuffer_expand_range(seg->flags, &startidx_exp, &endidx_exp);

    if(rd_i_flagsbuffer_has_code_n(seg->flags, startidx_exp,
                                   endidx_exp - startidx_exp))
        return false;

    if(!rd_i_undefine_n(ctx, address, sz, c)) return false;

    rd_i_type_unroll(ctx, address, &t);
    rd_i_db_set_type(ctx, address, &t, c);
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

/*
 * Resolve the entity at byte `offset` inside `type`.
 *
 * Because entities can share a starting byte (a struct's edge is also its
 * first member's edge, recursively: the coincidence stack), the question has
 * multiple valid answers:
 * - `min_depth` picks among them: it is a FLOOR,
 * - "refuse answers shallower than this many descents".
 * - 0 returns the shallowest entity at the offset;
 * - each increment peels one more edge;
 * - RD_MAX_DEPTH reaches the innermost leaf.
 *
 * Returns:
 *   false    malformed input (offset out of bounds, offset inside
 *            an opaque type, bad arguments).
 *            Logged, never panics: this is plugin facing.
 *
 *   true     out_r->field is the entity,
 *            out_r->depth the number of descents taken,
 *            out_r->item_idx the last array index crossed (if any).
 *            If out_r->depth < min_depth, the schema ended before
 *            the requested floor: there IS no deeper entity.
 *            This is a defined, expected outcome: it is how callers
 *            enumerate a coincidence stack to its end.
 *
 * - min_depth only ever skips OPTIONAL stops.
 * - offsets that fall inside a child force descent regardless,
 *   so the first genuine answer at an offset may sit deeper than 0
 *   (e.g. a member reached through an outer field at a nonzero relative
 *   offset).
 */
bool rd_type_resolve_offset(RDContext* ctx, const RDType* type, usize offset,
                            usize min_depth, RDResolveResult* out_r) {
    if(!type || !out_r) return false;

    *out_r = (RDResolveResult){0};

    if(!out_r) return false;
    return _rd_type_resolve(ctx, type, offset, min_depth, out_r);
}
