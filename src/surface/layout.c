#include "layout.h"
#include "core/context.h"
#include "io/flagsbuffer.h"
#include "support/error.h"
#include <inttypes.h>

#define RD_INDENT_FUNCTION 4
#define RD_INDENT_LABEL 6
#define RD_INDENT_CONTENT 8
#define RD_INDENT_DATA(d) (RD_INDENT_CONTENT + (((d) + 1) * 2))

static inline void _rd_layout_push(RDRowDescVect* out, RDRowKind kind,
                                   usize length, usize indent) {
    vect_push(out, (RDRowDesc){
                       .kind = kind,
                       .length = length,
                       .indent = indent,
                   });
}

static void _rd_layout_comments(RDContext* ctx, RDRenderFlags flags,
                                const RDSegmentFull* seg, usize idx,
                                RDCommentPlacement p, usize length,
                                usize indent, RDRowDescVect* out) {
    if(flags & RD_RF_NO_COMMENTS) return;
    if(!rd_i_flagsbuffer_has_comment(seg->flags, idx)) return;

    RDAddress address = seg->base.start_address + idx;
    usize n = rd_i_db_get_comment_count(ctx, address, p);

    RDRowKind kind = (p == RD_COMMENT_BEFORE) ? RD_ROWKIND_COMMENT_BEFORE
                                              : RD_ROWKIND_COMMENT_AFTER;

    for(usize i = 0; i < n; i++) {
        vect_push(out, (RDRowDesc){
                           .kind = kind,
                           .length = length,
                           .indent = indent,
                           .comment_idx = i,
                       });
    }
}

static usize _rd_layout_hexchunk_len(const RDSegmentFull* seg, usize idx) {
    usize curridx = idx;

    while(curridx < rd_flagsbuffer_get_length(seg->flags)) {
        if(!rd_flagsbuffer_has_unknown(seg->flags, curridx)) break;
        if(curridx != idx && rd_i_is_hexchunk_head(seg, curridx)) break;
        curridx++;
    }

    return curridx - idx;
}

static usize _rd_packed_element_count(const RDSegmentFull* seg, usize idx,
                                      usize elemsize, usize item_idx) {
    usize len = rd_flagsbuffer_get_length(seg->flags);
    usize n = 1;

    for(;;) {
        usize next = idx + (n * elemsize);
        if(next >= len) break;
        if(!rd_flagsbuffer_has_item(seg->flags, next)) break;

        if(rd_i_is_packed_element_head(seg, next, elemsize, item_idx + n))
            break;

        n++;
    }

    return n;
}

static usize _rd_layout_unknown(RDContext* ctx, RDRenderFlags flags,
                                const RDSegmentFull* seg, usize idx,
                                RDRowDescVect* out) {
    usize len = _rd_layout_hexchunk_len(seg, idx);

    _rd_layout_comments(ctx, flags, seg, idx, RD_COMMENT_BEFORE, len,
                        RD_INDENT_CONTENT, out);

    if(rd_i_flagsbuffer_has_xref_in(seg->flags, idx) ||
       rd_flagsbuffer_has_name(seg->flags, idx))
        _rd_layout_push(out, RD_ROWKIND_LABEL, len, RD_INDENT_LABEL);

    _rd_layout_push(out, RD_ROWKIND_HEXDUMP, len, RD_INDENT_CONTENT);
    _rd_layout_comments(ctx, flags, seg, idx, RD_COMMENT_AFTER, len,
                        RD_INDENT_CONTENT, out);
    return len;
}

static usize _rd_layout_code(RDContext* ctx, RDRenderFlags flags,
                             const RDSegmentFull* seg, usize idx,
                             RDRowDescVect* out) {
    usize len = rd_i_flagsbuffer_get_range_length(seg->flags, idx);

    _rd_layout_comments(ctx, flags, seg, idx, RD_COMMENT_BEFORE, len,
                        RD_INDENT_CONTENT, out);

    if(rd_flagsbuffer_has_func(seg->flags, idx)) {
        if(!(flags & RD_RF_NO_FUNCTION))
            _rd_layout_push(out, RD_ROWKIND_FUNCTION, len, RD_INDENT_FUNCTION);
    }
    else if(rd_i_flagsbuffer_has_xref_in(seg->flags, idx))
        _rd_layout_push(out, RD_ROWKIND_LABEL, len, RD_INDENT_LABEL);

    _rd_layout_push(out, RD_ROWKIND_INSTRUCTION, len, RD_INDENT_CONTENT);

    // the noret marker renders as a comment, so it follows the comment flag
    if(rd_flagsbuffer_has_noret(seg->flags, idx) &&
       !(flags & RD_RF_NO_COMMENTS))
        _rd_layout_push(out, RD_ROWKIND_NORET, len, RD_INDENT_CONTENT);

    _rd_layout_comments(ctx, flags, seg, idx, RD_COMMENT_AFTER, len,
                        RD_INDENT_CONTENT, out);
    return len;
}

static usize _rd_layout_data(RDContext* ctx, RDRenderFlags flags,
                             const RDSegmentFull* seg, usize idx,
                             RDRowDescVect* out) {
    RDDataHead head;
    rd_i_data_head_get(ctx, seg, idx, &head);

    usize whole_len = rd_type_size(&head.root, ctx);
    bool array_of_compounds = head.root.count > 0 &&
                              head.root.mod == RD_TYPE_NONE &&
                              rd_i_typedef_is_compound(head.root.def);

    bool has_banner = head.has_banner && !array_of_compounds;

    if(!rd_i_type_resolve_chain(ctx, &head.root, head.offset,
                                &ctx->resolve_buf))
        return whole_len;

    usize head_indent = RD_INDENT_CONTENT;

    if(!has_banner) {
        const RDResolveResult* first;

        vect_each(first, &ctx->resolve_buf) {
            if(first->at_offset) {
                head_indent = RD_INDENT_DATA(first->depth);
                break;
            }
        }
    }

    _rd_layout_comments(ctx, flags, seg, idx, RD_COMMENT_BEFORE, whole_len,
                        head_indent, out);

    usize advance = whole_len;

    if(has_banner) {
        RDRowDesc d = {
            .kind = RD_ROWKIND_DATA_BANNER,
            .length = whole_len,
            .indent = RD_INDENT_CONTENT,
            .resolve = {.field = {.type = head.root, .name = NULL}},
        };

        vect_push(out, d);

        // a solid root: the banner is the whole rendering
        if(!rd_i_type_has_more(&head.root)) {
            _rd_layout_comments(ctx, flags, seg, idx, RD_COMMENT_AFTER,
                                whole_len, head_indent, out);
            return whole_len;
        }
    }

    bool is_string = rd_type_is_string(&head.root);

    const RDResolveResult* res;
    vect_each(res, &ctx->resolve_buf) {
        if(!res->at_offset) continue;

        usize elem_size = rd_type_size(&res->field.type, ctx);

        if(!is_string && elem_size && rd_i_link_is_packable(res) &&
           rd_i_is_packed_element_head(seg, idx, elem_size,
                                       res->item_idx.value)) {
            usize n = _rd_packed_element_count(seg, idx, elem_size,
                                               res->item_idx.value);

            RDRowDesc d = {
                .kind = RD_ROWKIND_DATA_ELEMENTS,
                .length = n * elem_size,
                .indent = RD_INDENT_DATA(res->depth),
                .elements = {.type = res->field.type, .count = n},
            };

            vect_push(out, d);
            advance = d.length;
            break;
        }

        RDRowDesc d = {
            .kind = RD_ROWKIND_DATA_LINK,
            .length = elem_size,
            .indent = RD_INDENT_DATA(res->depth),
            .resolve = *res,
        };

        vect_push(out, d);

        advance = elem_size;

        if(is_string) break;
    }

    _rd_layout_comments(ctx, flags, seg, idx, RD_COMMENT_AFTER, advance,
                        head_indent, out);

    return advance;
}

bool rd_i_is_hexchunk_head(const RDSegmentFull* seg, usize idx) {
    if(!rd_flagsbuffer_has_unknown(seg->flags, idx)) return false;
    if(idx == 0) return true;
    if(!rd_flagsbuffer_has_unknown(seg->flags, idx - 1)) return true;

    if(((seg->base.start_address + idx) % RD_SURFACE_HEX_LINE) == 0)
        return true;

    return rd_i_flagsbuffer_has_info(seg->flags, idx);
}

bool rd_i_link_is_packable(const RDResolveResult* res) {
    if(!res->item_idx.has_value) return false; // not an element
    if(res->field.name) return false;          // a named member, not an element
    if(res->field.type.count > 0) return false; // an array, not an element
    if(rd_type_is_ptr(&res->field.type)) return false;
    return res->field.type.def->kind == RD_TKIND_PRIM;
}

bool rd_i_is_packed_element_head(const RDSegmentFull* seg, usize idx,
                                 usize elem_size, usize item_idx) {
    if(!item_idx) return true; // the array's first element
    if(rd_i_flagsbuffer_has_info(seg->flags, idx)) return true;

    if(((seg->base.start_address + idx) % RD_SURFACE_HEX_LINE) == 0)
        return true;

    // the element right after a broken-out one restarts the run
    return idx >= elem_size &&
           rd_i_flagsbuffer_has_info(seg->flags, idx - elem_size);
}

void rd_i_data_head_get(RDContext* ctx, const RDSegmentFull* seg, usize idx,
                        RDDataHead* out) {
    RDAddress address = seg->base.start_address + idx;

    if(rd_flagsbuffer_has_type(seg->flags, idx)) {
        RDTypeFull t;
        bool got = rd_i_db_get_type(ctx, address, &t);
        panic_if(!got, "type not found @ %s:%x", seg->base.name, address);

        *out = (RDDataHead){.root = t.base, .offset = 0, .has_banner = true};
        return;
    }

    if(rd_flagsbuffer_has_field(seg->flags, idx) ||
       rd_flagsbuffer_has_item(seg->flags, idx)) {
        RDAddress root_address = address;
        RDType root;
        bool got = rd_i_db_get_root_type(ctx, &root_address, &root);
        panic_if(!got, "root type not found @ %s:%x", seg->base.name, address);

        *out = (RDDataHead){.root = root, .offset = address - root_address};
        return;
    }

    unreachable();
}

usize rd_i_item_layout(RDContext* ctx, RDRenderFlags flags,
                       const RDSegmentFull* seg, usize idx,
                       RDRowDescVect* out) {
    vect_clear(out);

    panic_if(rd_flagsbuffer_has_tail(seg->flags, idx),
             "tail detected @ %" PRIX64, seg->base.start_address + idx);

    if(!idx && !(flags & RD_RF_NO_SEGMENT))
        _rd_layout_push(out, RD_ROWKIND_SEGMENT, 0, 0);

    usize advance;

    if(rd_flagsbuffer_has_unknown(seg->flags, idx))
        advance = _rd_layout_unknown(ctx, flags, seg, idx, out);
    else if(rd_flagsbuffer_has_data(seg->flags, idx))
        advance = _rd_layout_data(ctx, flags, seg, idx, out);
    else if(rd_flagsbuffer_has_code(seg->flags, idx))
        advance = _rd_layout_code(ctx, flags, seg, idx, out);
    else
        unreachable();

    panic_if(vect_is_empty(out), "empty layout @ %" PRIX64,
             seg->base.start_address + idx);
    panic_if(!advance, "zero advance @ %" PRIX64,
             seg->base.start_address + idx);

    return advance;
}

usize rd_i_item_layout_content(const RDRowDescVect* rows) {
    for(usize i = 0; i < vect_length(rows); i++) {
        switch(vect_at(rows, i)->kind) {
            case RD_ROWKIND_INSTRUCTION:
            case RD_ROWKIND_HEXDUMP:
            case RD_ROWKIND_DATA_BANNER:
            case RD_ROWKIND_DATA_LINK:
            case RD_ROWKIND_DATA_ELEMENTS: return i;
            default: break;
        }
    }

    return 0;
}
