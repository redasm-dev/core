#include "row.h"
#include "core/context.h"
#include "io/flagsbuffer.h"
#include "support/error.h"
#include "surface/items.h"
#include <inttypes.h>

/*
 * The CODE slot layout, single source of truth
 * mirror of _rd_render_item_code, keep in lockstep.
 *
 *  Consumers: the renderer's dispatch, backward stepping (below), and the
 *             jump-arrow builder (path.c), which needs to target the
 *             instruction row specifically.
 */
usize rd_i_row_code_instr_sub_line(const RDSegmentFull* seg, usize idx) {
    return (rd_flagsbuffer_has_func(seg->flags, idx) ||
            rd_i_flagsbuffer_has_xref_in(seg->flags, idx))
               ? 1
               : 0;
}

static usize _rd_code_last_sub_line(const RDSegmentFull* seg, usize idx) {
    usize last = rd_i_row_code_instr_sub_line(seg, idx);
    if(rd_flagsbuffer_has_noret(seg->flags, idx)) last++;
    return last;
}

/*
 * The last row ordinal at a DATA head, counted by the SAME function the
 * forward dispatch uses to render them (rd_i_data_chain_row): the last
 * link that exists, shifted by one when a banner occupies row 0. Never
 * derive this from a raw resolver depth.
 * Row ordinals and schema depths are different quantities (a field reached
 * through an outer member at a nonzero offset is row 0 at its own head, yet
 * sits at depth >= 1).
 */
static usize _rd_data_last_sub_line(RDContext* ctx, const RDSegmentFull* seg,
                                    usize idx) {
    RDDataHead head;
    rd_i_data_head_get(ctx, seg, idx, &head);

    // a banner over a solid root is the whole rendering: one row
    if(head.has_banner && !rd_i_type_has_more(&head.root)) return 0;

    usize first = head.has_banner ? 1 : 0; // the banner occupies row 0

    RDResolveResult res;
    bool ok = rd_i_data_chain_row(ctx, &head, 0, &res);
    panic_if(!ok, "unresolvable data head @ %s+%zx", seg->base.name, idx);

    usize last = first; // chain[0] always exists past this point
    while(rd_i_data_chain_row(ctx, &head, (last - first) + 1, &res))
        last++;

    return last;
}

// mirror of _rd_render_item_unknown's slot layout, keep in lockstep
static usize _rd_unknown_last_sub_line(const RDSegmentFull* seg, usize idx) {
    // rows: [optional label], hex line - the hex line is always last
    return rd_i_flagsbuffer_has_info(seg->flags, idx) ? 1 : 0;
}

void rd_i_rowvect_destroy(RDRowVect* self) {
    RDRow* row;
    vect_each(row, self) {
        vect_destroy(&row->data);
        vect_destroy(&row->cells);
    }
}

void rd_i_rowvect_push(RDContext* ctx, RDRowVect* self, usize sub_line,
                       RDAddress address) {
    RDRow r = {
        .address = address,
        .sub_line = sub_line,
        .curr_data = rd_i_default_cell_data(),
    };

    const RDSegmentFull* seg = rd_i_db_find_segment(ctx, address);
    panic_if(!seg, "segment not found @ %" PRIx64, address);
    usize idx = rd_i_address2index(seg, address);

    r.curr_data.is_instruction = rd_flagsbuffer_has_code(seg->flags, idx);
    vect_push(self, r);
}

void rd_i_row_reserve(RDRow* self, int n) {
    vect_reserve(&self->cells, (usize)n);
    vect_reserve(&self->data, (usize)n);
}

void rd_i_row_push(RDRow* self, u32 cp, RDThemeKind fg, RDThemeKind bg) {
    assert(vect_length(&self->cells) == (vect_length(&self->data)));

    if(fg == RD_THEME_DEFAULT) fg = RD_THEME_FOREGROUND;
    if(bg == RD_THEME_DEFAULT) bg = RD_THEME_BACKGROUND;
    vect_push(&self->cells, (RDCell){.cp = cp, .fg = fg, .bg = bg});
    vect_push(&self->data, self->curr_data);
}

bool rd_i_row_step_back(RDContext* ctx, const RDSegmentFull** seg,
                        usize* seg_idx, usize* idx, usize* sub_line) {
    const RDSegmentFullVect* segments = rd_i_db_get_segments(ctx);

    // (1) already sitting past the segment row's own slot?
    // just go one sub_line shallower, same address, no flags touched at all.
    if(*sub_line != RD_SUB_LINE_NONE && *sub_line > 0) {
        (*sub_line)--;
        return true;
    }

    // (2) at sub_line 0 of this address
    // one more step back is the segment row itself, IF this address owns one.
    // Same idx, sentinel.
    if(*sub_line == 0 && *idx == 0) {
        *sub_line = RD_SUB_LINE_NONE;
        return true;
    }

    // (3) either already at the segment row (RD_SUB_LINE_NONE), or this
    // address never had one, genuinely cross to a new address.
    if((*idx) == 0) {
        if(*seg_idx == 0) return false;

        (*seg_idx)--;
        *seg = *vect_at(segments, *seg_idx);
        *idx = rd_flagsbuffer_get_length((*seg)->flags);
    }

    (*idx)--;

    if(rd_flagsbuffer_has_unknown((*seg)->flags, *idx)) {
        /*
         * mirror of _rd_render_item_unknown's chunking, keep in lockstep.
         * A chunk head is: a segment-relative alignment boundary, the
         * first byte of an unknown run, or a labeled byte.
         * All three are recomputable locally, so the walk is bounded
         * by one hex line
         */
        while(*idx > 0) {
            bool is_chunk_head =
                (*idx % RD_SURFACE_HEX_LINE) == 0 ||
                rd_i_flagsbuffer_has_info((*seg)->flags, *idx) ||
                !rd_flagsbuffer_has_unknown((*seg)->flags, *idx - 1);

            if(is_chunk_head) break;
            (*idx)--;
        }
    }
    else if(rd_flagsbuffer_has_tail((*seg)->flags, *idx)) {
        while(*idx > 0 && rd_flagsbuffer_has_tail((*seg)->flags, *idx))
            (*idx)--;

        panic_if(rd_flagsbuffer_has_tail((*seg)->flags, *idx),
                 "item spans segment boundary");
    }

    if(rd_flagsbuffer_has_code((*seg)->flags, *idx))
        *sub_line = _rd_code_last_sub_line(*seg, *idx);
    else if(rd_flagsbuffer_has_unknown((*seg)->flags, *idx))
        *sub_line = _rd_unknown_last_sub_line(*seg, *idx);
    else if(rd_flagsbuffer_has_data((*seg)->flags, *idx))
        *sub_line = _rd_data_last_sub_line(ctx, *seg, *idx);
    else
        unreachable();

    return true;
}
