#include "type.h"
#include "core/context.h"
#include "io/flagsbuffer.h"
#include "support/error.h"
#include "types/def.h"
#include <string.h>

usize rd_size_of(const RDContext* ctx, const char* name, usize n) {
    return rd_i_size_of(ctx, name, n, RD_TYPE_NONE);
}

usize rd_i_size_of(const RDContext* ctx, const char* name, usize n,
                   RDTypeFlags flags) {
    RDTypeDef* tdef = rd_i_typedef_find(ctx, name, true);
    usize sz;

    if(tdef->kind == RD_TKIND_FUNC) {
        sz = rd_processor_get_code_ptr_size(ctx);
        if(!sz) sz = rd_processor_get_ptr_size(ctx);
    }
    else if(flags & RD_TYPE_ISPOINTER) {
        sz = rd_processor_get_ptr_size(ctx);
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

bool rd_i_set_type(RDContext* ctx, RDAddress address, const char* name, usize n,
                   RDTypeFlags flags, RDConfidence c) {
    const RDSegmentFull* seg = rd_i_find_segment(ctx, address);
    if(!seg) return false;

    usize newsz = rd_i_size_of(ctx, name, n, flags);
    if(!newsz) return false;

    usize idx = rd_i_address2index(seg, address);
    usize startidx_exp = idx;
    usize endidx_exp = startidx_exp + newsz;
    rd_i_flagsbuffer_expand_range(seg->flags, &startidx_exp, &endidx_exp);

    if(rd_i_flagsbuffer_has_code_n(seg->flags, startidx_exp,
                                   endidx_exp - startidx_exp))
        return false;

    // Walk all type HEADs in the expanded range.
    // Confidence of the range is the max confidence of any member, not just
    // the head.
    // Any single item outranking c blocks the whole operation.
    usize i = startidx_exp;
    while(i < endidx_exp) {
        if(rd_i_flagsbuffer_has_type(seg->flags, i)) {
            RDAddress a = rd_i_index2address(seg, i);
            RDTypeFull oldt;
            bool ok = rd_i_db_get_type(ctx, a, &oldt);
            assert(ok && "type flag set but not in DB");

            if(oldt.confidence > c) return false; // confidence wins over size

            if(oldt.confidence == c && i == idx) {
                usize oldsz = rd_i_size_of(ctx, oldt.base.name, oldt.base.count,
                                           oldt.base.flags);

                if(newsz <= oldsz && !strcmp(name, oldt.base.name))
                    return false;

                // different type name or larger: allow replacement
            }

            i += rd_i_flagsbuffer_get_range_length(seg->flags, i);
        }
        else {
            i++;
        }
    }

    // startidx_exp is always a valid HEAD index, safe to convert.
    // endidx_exp is an exclusive upper bound, may equal flags->base.length,
    // which equals end_address.
    // Compute arithmetically to avoid the assertion.
    RDAddress startaddr_exp = rd_i_index2address(seg, startidx_exp);
    RDAddress endaddr_exp = startaddr_exp + (endidx_exp - startidx_exp);
    rd_i_db_del_type_range(ctx, startaddr_exp, endaddr_exp);

    rd_i_flagsbuffer_undefine(seg->flags, startidx_exp,
                              endidx_exp - startidx_exp);

    rd_i_flagsbuffer_set_type(seg->flags, idx, newsz);
    rd_i_db_set_type(ctx, address, name, n, flags, c);
    return true;
}

bool rd_i_get_type(RDContext* ctx, RDAddress address, RDTypeFull* t) {
    const RDSegmentFull* seg = rd_i_find_segment(ctx, address);
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
                  RDTypeFlags flags) {
    return rd_i_set_type(ctx, address, name, n, flags, RD_CONFIDENCE_AUTO);
}

bool rd_library_type(RDContext* ctx, RDAddress address, const char* name,
                     usize n, RDTypeFlags flags) {
    return rd_i_set_type(ctx, address, name, n, flags, RD_CONFIDENCE_LIBRARY);
}

bool rd_user_type(RDContext* ctx, RDAddress address, const char* name, usize n,
                  RDTypeFlags flags) {
    return rd_i_set_type(ctx, address, name, n, flags, RD_CONFIDENCE_USER);
}
