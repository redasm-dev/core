#include "type.h"
#include "core/context.h"
#include "io/flagsbuffer.h"
#include "support/error.h"
#include "support/logging.h"
#include "types/def.h"

usize rd_i_size_of(const RDContext* ctx, const char* name, usize n,
                   RDTypeModifier mod) {
    const RDProcessorPlugin* p = ctx->processorplugin;
    RDTypeDef* tdef = rd_i_typedef_find(ctx, name);

    if(!tdef) {
        LOG_FAIL("cannot get the size of '%s', type not found", name);
        return 0;
    }

    usize sz;

    if(tdef->kind == RD_TKIND_FUNC || mod == RD_TYPE_CPTR) {
        sz = p->code_ptr_size;
        if(!sz) sz = p->ptr_size;
    }
    else if(mod == RD_TYPE_PTR) {
        sz = p->ptr_size;
    }
    else {
        if(!tdef->size) rd_i_typedef_resolve_size(ctx, tdef);
        panic_if(!tdef->size, "type '%s' has unresolved size", name);
        sz = tdef->size;
    }

    return n > 0 ? sz * n : sz;
}

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

const char* rd_i_type_to_str(const RDType* t, RDCharVect* buf) {
    if(rd_i_type_is_void(t)) return "void";

    assert(t->name);

    str_clear(buf);
    str_append(buf, t->name);

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
                   RDTypeModifier flags, RDConfidence c) {
    const RDSegmentFull* seg = rd_i_db_find_segment(ctx, address);
    if(!seg) return false;

    usize sz = rd_i_size_of(ctx, name, n, flags);
    if(!sz) return false;

    usize idx = rd_i_address2index(seg, address);
    usize startidx_exp = idx, endidx_exp = startidx_exp + sz;
    rd_i_flagsbuffer_expand_range(seg->flags, &startidx_exp, &endidx_exp);

    if(rd_i_flagsbuffer_has_code_n(seg->flags, startidx_exp,
                                   endidx_exp - startidx_exp))
        return false;

    if(!rd_i_undefine_n(ctx, address, sz, c)) return false;

    rd_i_flagsbuffer_set_type(seg->flags, idx, sz);
    rd_i_db_set_type(ctx, address, name, n, flags, c);
    return true;
}

bool rd_i_get_type(RDContext* ctx, RDAddress address, RDTypeFull* t) {
    const RDSegmentFull* seg = rd_i_db_find_segment(ctx, address);
    if(!seg) return false;

    usize idx = rd_i_address2index(seg, address);
    if(!rd_i_flagsbuffer_has_type(seg->flags, idx)) return false;

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
