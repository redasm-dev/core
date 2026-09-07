#include "layout.h"
#include "core/context.h"
#include "io/flagsbuffer.h"
#include "support/error.h"
#include "surface/row.h" // RD_SURFACE_HEX_LINE
#include <inttypes.h>

bool rd_i_is_hexchunk_head(const RDSegmentFull* seg, usize idx) {
    if(!rd_flagsbuffer_has_unknown(seg->flags, idx)) return false;
    if(idx == 0) return true;
    if(!rd_flagsbuffer_has_unknown(seg->flags, idx - 1)) return true;

    if(((seg->base.start_address + idx) % RD_SURFACE_HEX_LINE) == 0)
        return true;

    return rd_i_flagsbuffer_has_info(seg->flags, idx);
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

bool rd_i_data_chain_row(RDContext* ctx, const RDDataHead* head, usize link,
                         RDResolveResult* out) {
    RDResolveResult probe;
    if(!rd_type_resolve_offset(ctx, &head->root, head->offset, 0, &probe))
        return false;

    if(link == 0) {
        *out = probe;
        return true;
    }

    usize want = probe.depth + link;
    if(!rd_type_resolve_offset(ctx, &head->root, head->offset, want, out))
        return false;

    return out->depth == want; // shallower: the chain ended before this link
}

static void _rd_layout_push(RDRowDescVect* out, RDRowKind kind, usize length) {
    RDRowDesc d = {.kind = kind, .length = length};
    vect_push(out, d);
}

static void _rd_layout_comments(RDContext* ctx, RDRenderFlags flags,
                                const RDSegmentFull* seg, usize idx,
                                RDCommentPlacement p, usize length,
                                RDRowDescVect* out) {
    if(flags & RD_RF_NO_COMMENTS) return;
    if(!rd_i_flagsbuffer_has_comment(seg->flags, idx)) return;

    RDAddress address = seg->base.start_address + idx;
    usize n = rd_i_db_get_comment_count(ctx, address, p);

    RDRowKind kind = (p == RD_COMMENT_BEFORE) ? RD_ROWKIND_COMMENT_BEFORE
                                              : RD_ROWKIND_COMMENT_AFTER;

    for(usize i = 0; i < n; i++) {
        RDRowDesc d = {.kind = kind, .length = length, .comment_idx = i};
        vect_push(out, d);
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

static usize _rd_layout_unknown(RDContext* ctx, RDRenderFlags flags,
                                const RDSegmentFull* seg, usize idx,
                                RDRowDescVect* out) {
    usize len = _rd_layout_hexchunk_len(seg, idx);

    _rd_layout_comments(ctx, flags, seg, idx, RD_COMMENT_BEFORE, len, out);

    if(rd_i_flagsbuffer_has_xref_in(seg->flags, idx) ||
       rd_flagsbuffer_has_name(seg->flags, idx))
        _rd_layout_push(out, RD_ROWKIND_LABEL, len);

    _rd_layout_push(out, RD_ROWKIND_HEXDUMP, len);
    _rd_layout_comments(ctx, flags, seg, idx, RD_COMMENT_AFTER, len, out);
    return len;
}

static usize _rd_layout_code(RDContext* ctx, RDRenderFlags flags,
                             const RDSegmentFull* seg, usize idx,
                             RDRowDescVect* out) {
    usize len = rd_i_flagsbuffer_get_range_length(seg->flags, idx);

    _rd_layout_comments(ctx, flags, seg, idx, RD_COMMENT_BEFORE, len, out);

    if(rd_flagsbuffer_has_func(seg->flags, idx)) {
        if(!(flags & RD_RF_NO_FUNCTION))
            _rd_layout_push(out, RD_ROWKIND_FUNCTION, len);
    }
    else if(rd_i_flagsbuffer_has_xref_in(seg->flags, idx))
        _rd_layout_push(out, RD_ROWKIND_LABEL, len);

    _rd_layout_push(out, RD_ROWKIND_INSTRUCTION, len);

    // the noret marker renders as a comment, so it follows the comment flag
    if(rd_flagsbuffer_has_noret(seg->flags, idx) &&
       !(flags & RD_RF_NO_COMMENTS))
        _rd_layout_push(out, RD_ROWKIND_NORET, len);

    _rd_layout_comments(ctx, flags, seg, idx, RD_COMMENT_AFTER, len, out);
    return len;
}

// --- data ---

static usize _rd_layout_data(RDContext* ctx, RDRenderFlags flags,
                             const RDSegmentFull* seg, usize idx,
                             RDRowDescVect* out) {
    RDDataHead head;
    rd_i_data_head_get(ctx, seg, idx, &head);

    usize whole_len = rd_type_size(&head.root, ctx);
    _rd_layout_comments(ctx, flags, seg, idx, RD_COMMENT_BEFORE, whole_len,
                        out);

    usize advance = whole_len;

    if(head.has_banner) {
        RDRowDesc d = {
            .kind = RD_ROWKIND_DATA_BANNER,
            .length = whole_len,
            .resolve = {.field = {.type = head.root, .name = NULL}},
        };

        vect_push(out, d);

        // a solid root: the banner is the whole rendering
        if(!rd_i_type_has_more(&head.root)) {
            _rd_layout_comments(ctx, flags, seg, idx, RD_COMMENT_AFTER,
                                whole_len, out);
            return whole_len;
        }
    }

    bool is_string = rd_type_is_string(&head.root);

    for(usize link = 0;; link++) {
        RDResolveResult res;
        if(!rd_i_data_chain_row(ctx, &head, link, &res)) break;

        RDRowDesc d = {
            .kind = RD_ROWKIND_DATA_LINK,
            .length = rd_type_size(&res.field.type, ctx),
            .resolve = res,
        };

        vect_push(out, d);
        advance = d.length;

        if(is_string) break;
    }

    _rd_layout_comments(ctx, flags, seg, idx, RD_COMMENT_AFTER, advance, out);
    return advance;
}

usize rd_i_item_layout(RDContext* ctx, RDRenderFlags flags,
                       const RDSegmentFull* seg, usize idx,
                       RDRowDescVect* out) {
    vect_clear(out);

    panic_if(rd_flagsbuffer_has_tail(seg->flags, idx),
             "tail detected @ %" PRIX64, seg->base.start_address + idx);

    if(!idx && !(flags & RD_RF_NO_SEGMENT))
        _rd_layout_push(out, RD_ROWKIND_SEGMENT, 0);

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
            case RD_ROWKIND_DATA_LINK: return i;
            default: break;
        }
    }

    return 0;
}
