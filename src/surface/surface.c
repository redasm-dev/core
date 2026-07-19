#include "core/context.h"
#include "io/flagsbuffer.h"
#include "support/containers.h"
#include "surface/items.h"
#include "surface/path.h"
#include "surface/renderer.h"
#include "surface/state.h"
#include <redasm/surface/surface.h>

typedef struct RDSurface {
    RDRenderer* renderer;
    RDSurfaceState state;
    RDSurfacePath path_builder;
    usize max_rows;
} RDSurface;

static void _rd_surface_render_finalize(RDSurface* self) {
    rd_i_renderer_fill_columns(self->renderer);
    rd_i_renderer_highlight_row(self->renderer, self->state.pos.row);

    if(!rd_surface_has_selection(self)) {
        rd_i_renderer_highlight_words(self->renderer, self->state.pos.row,
                                      self->state.pos.col);
    }

    RDSurfacePos startsel = rd_i_surfacestate_get_start_selection(&self->state);
    RDSurfacePos endsel = rd_i_surfacestate_get_end_selection(&self->state);

    rd_i_renderer_highlight_selection(self->renderer, startsel.row,
                                      startsel.col, endsel.row, endsel.col);

    rd_i_renderer_highlight_cursor(self->renderer, self->state.pos.row,
                                   self->state.pos.col);

    rd_i_renderer_swap(self->renderer);
}

static bool _rd_surface_render(RDSurface* self, usize seg_idx, usize idx,
                               usize sub_line) {
    const RDSegmentFullVect* segments =
        rd_i_db_get_segments(self->renderer->context);

    if(seg_idx == vect_length(segments)) return false;

    const RDSegmentFull* seg = *vect_at(segments, seg_idx);

    while(seg_idx < vect_length(segments)) {
        usize nrows = vect_length(&self->renderer->rows_back);
        if(nrows >= self->max_rows) break;

        seg = *vect_at(segments, seg_idx);

        // fixup to head
        if(rd_flagsbuffer_has_tail(seg->flags, idx))
            rd_i_flagsbuffer_expand_range(seg->flags, &idx, NULL);

        if(sub_line == RD_SUB_LINE_NONE) {
            // the segment banner is owed only at the segment's first byte.
            // Any other landing simply starts at the head's row 0.
            // Never let RD_SUB_LINE_NONE reach the dispatch
            if(idx == 0) rd_i_render_segment_item(self->renderer, seg);
            sub_line = 0;
        }

        usize last_len = 0;

        /*
         * One head can yield many rows. Dispatch contract:
         *   OK        -> the row exists; .length is its byte width
         *   EXHAUSTED -> no such row; advance idx by the LAST OK row's
         *                width (for DATA that is the chain's deepest,
         *                narrowest entity, exactly the step to the
         *                next head).
         *                Every head must yield a row at sub_line 0.
         */
        while(idx < rd_flagsbuffer_get_length(seg->flags)) {
            if(vect_length(&self->renderer->rows_back) >= self->max_rows) break;

            RDRenderItemResult r =
                rd_i_render_item(self->renderer, seg, idx, sub_line);

            if(r.status == RD_ROW_OK) {
                // stamp the row's byte width for rd_surface_get_byte_span.
                // Some renderers return OK without emitting a row under
                // RD_RF_* flags, hence the count check
                if(vect_length(&self->renderer->rows_back) > nrows) {
                    vect_last(&self->renderer->rows_back)->bytes_length =
                        r.length;
                }

                last_len = r.length;
                sub_line++;
            }
            else {
                sub_line = 0;
                idx += last_len;
            }
        }

        sub_line = RD_SUB_LINE_NONE;
        seg_idx++;
        idx = 0;
    }

    return true;
}

/*
 * Find the first visible row for 'address', or -1.
 * Self-contained scan over rows_front so the in-viewport test cannot depend on
 * renderer index semantics.
 * Banner rows are excluded: they share the segment start address but are not
 * the item at that address.
 */
static int _rd_surface_find_row(const RDSurface* self, RDAddress address) {
    int i = 0;

    const RDRow* row;
    vect_each(row, &self->renderer->rows_front) {
        if(row->sub_line != RD_SUB_LINE_NONE && row->address == address)
            return i;
        i++;
    }

    return -1;
}

/*
 * Every path that fills the viewport goes through here, so the state
 * anchor (start + start_sub_line) is ALWAYS the actual current top:
 * render, scroll, jump and history restore cannot drift from it.
 */
static bool _rd_surface_render_from(RDSurface* self, const RDSegmentFull* seg,
                                    usize seg_idx, usize idx, usize sub_line) {
    self->state.start = seg->base.start_address + idx;
    self->state.start_sub_line = sub_line;

    rd_i_renderer_clear(self->renderer);
    bool ok = _rd_surface_render(self, seg_idx, idx, sub_line);
    _rd_surface_render_finalize(self);
    return ok;
}

static bool _rd_surface_render_at(RDSurface* self, RDAddress address,
                                  usize sub_line) {
    RDContext* ctx = self->renderer->context;

    usize seg_idx;
    if(!rd_i_db_find_segment_index(ctx, address, &seg_idx)) return false;

    const RDSegmentFullVect* segments = rd_i_db_get_segments(ctx);
    const RDSegmentFull* seg = *vect_at(segments, seg_idx);
    usize idx = rd_i_address2index(seg, address);

    return _rd_surface_render_from(self, seg, seg_idx, idx, sub_line);
}

RDSurface* rd_surface_create(RDContext* ctx, RDRenderFlags flags) {
    RDSurface* self = rd_alloc(sizeof(*self));

    *self = (RDSurface){
        .renderer = rd_i_renderer_create(ctx, flags),
    };

    return self;
}

void rd_surface_destroy(RDSurface* self) {
    if(!self) return;
    rd_i_surfacepath_deinit(&self->path_builder);
    rd_i_surfacestate_deinit(&self->state);
    rd_i_renderer_destroy(self->renderer);
    rd_free(self);
}

void rd_surface_set_highlight_word(RDSurface* self, const char* word) {
    rd_i_renderer_set_highlight_word(self->renderer, word);
}

void rd_surface_set_max_rows(RDSurface* self, usize rows) {
    self->max_rows = rows;
}

bool rd_surface_set_pos(RDSurface* self, int row, int col) {
    rd_i_renderer_fit(self->renderer, &row, &col);
    return rd_i_surfacestate_set_pos(&self->state, row, col);
}

void rd_surface_set_mode(RDSurface* self, RDRenderMode m) {
    rd_i_renderer_set_mode(self->renderer, m);
}

void rd_surface_set_cursor_visible(RDSurface* self, bool b) {
    rd_i_renderer_set_cursor_visible(self->renderer, b);
}

void rd_surface_set_columns(RDSurface* self, int cols) {
    self->renderer->columns = cols;
}

RDRenderMode rd_surface_get_mode(const RDSurface* self) {
    return rd_i_renderer_get_mode(self->renderer);
}

bool rd_surface_has_selection(const RDSurface* self) {
    return rd_i_surfacestate_has_selection(&self->state);
}

bool rd_surface_can_go_back(const RDSurface* self) {
    return rd_i_surfacestate_can_go_back(&self->state);
}

bool rd_surface_can_go_forward(const RDSurface* self) {
    return rd_i_surfacestate_can_go_forward(&self->state);
}

bool rd_surface_go_back(RDSurface* self) {
    if(!rd_i_surfacestate_go_back(&self->state)) return false;

    return _rd_surface_render_at(self, self->state.start,
                                 self->state.start_sub_line);
}

bool rd_surface_go_forward(RDSurface* self) {
    if(!rd_i_surfacestate_go_forward(&self->state)) return false;

    return _rd_surface_render_at(self, self->state.start,
                                 self->state.start_sub_line);
}

void rd_surface_clear_history(RDSurface* self) {
    rd_i_surfacestate_clear_history(&self->state);
}

void rd_surface_clear_selection(RDSurface* self) {
    rd_i_surfacestate_clear_selection(&self->state);
}

RDSurfacePos rd_surface_get_pos(const RDSurface* self) {
    return self->state.pos;
}

const char* rd_surface_get_word_under_pos(const RDSurface* self,
                                          const RDSurfacePos* pos) {
    return rd_i_renderer_get_word_under_pos(self->renderer, pos);
}

bool rd_surface_get_address_under_pos(const RDSurface* self,
                                      const RDSurfacePos* pos,
                                      RDAddress* address) {
    return rd_i_renderer_get_address_under_pos(self->renderer, pos, address);
}

bool rd_surface_get_address_under_cursor(const RDSurface* self,
                                         RDAddress* address) {
    return rd_surface_get_address_under_pos(self, &self->state.pos, address);
}

bool rd_surface_get_cell_data_under_cursor(const RDSurface* self,
                                           RDCellData* cd) {
    return rd_surface_get_cell_data_under_pos(self, &self->state.pos, cd);
}

bool rd_surface_get_cell_data_under_pos(const RDSurface* self,
                                        const RDSurfacePos* pos,
                                        RDCellData* cd) {
    return rd_i_renderer_get_cell_data_under_pos(self->renderer, pos, cd);
}

bool rd_surface_render(RDSurface* self, RDAddress address) {
    // repaint path: no history semantics (jumps go through
    // rd_surface_jump_to, which pushes history and centers)
    return _rd_surface_render_at(self, address, RD_SUB_LINE_NONE);
}

bool rd_surface_repaint(RDSurface* self) {
    return _rd_surface_render_at(self, self->state.start,
                                 self->state.start_sub_line);
}

bool rd_surface_scroll(RDSurface* self, int n) {
    if(!n || vect_is_empty(&self->renderer->rows_front)) return false;

    RDContext* ctx = self->renderer->context;
    RDRowVect* front = &self->renderer->rows_front;

    if(n > 0) {
        RDRow* top = vect_first(front);

        usize seg_idx;
        if(!rd_i_db_find_segment_index(ctx, top->address, &seg_idx))
            return false;

        const RDSegmentFullVect* segments = rd_i_db_get_segments(ctx);
        const RDSegmentFull* seg = *vect_at(segments, seg_idx);
        usize idx = rd_i_address2index(seg, top->address);

        usize saved = self->max_rows;
        self->max_rows = (usize)n + saved;
        rd_i_renderer_clear(self->renderer);
        _rd_surface_render(self, seg_idx, idx, top->sub_line);
        self->max_rows = saved;

        usize got = vect_length(&self->renderer->rows_back);

        // everything to the end is already visible: nothing to scroll
        // (rows_front untouched, the stale probe in rows_back is cleared
        // by whichever render runs next)
        if(got <= saved) return false;

        usize new_top = (usize)n;
        if(got < (usize)n + saved) new_top = got - saved; // bottom clamp

        RDRow* r = vect_at(&self->renderer->rows_back, new_top);
        RDAddress address = r->address;
        usize sub_line = r->sub_line; // copy out: the render clears the probe

        return _rd_surface_render_at(self, address, sub_line);
    }

    // backward: step in flag space from the current top
    RDRow* r = vect_first(front);

    usize seg_idx;
    if(!rd_i_db_find_segment_index(ctx, r->address, &seg_idx)) return false;

    const RDSegmentFullVect* segments =
        rd_i_db_get_segments(self->renderer->context);

    const RDSegmentFull* seg = *vect_at(segments, seg_idx);
    usize idx = rd_i_address2index(seg, r->address);
    usize sub_line = r->sub_line;

    bool moved = false;

    for(; n < 0; n++) {
        if(!rd_i_row_step_back(ctx, &seg, &seg_idx, &idx, &sub_line)) break;
        moved = true;
    }

    if(!moved) return false; // already at the very top: end-stop
    return _rd_surface_render_from(self, seg, seg_idx, idx, sub_line);
}

bool rd_surface_jump_to(RDSurface* self, RDAddress address) {
    RDContext* ctx = self->renderer->context;

    usize seg_idx;
    if(!rd_i_db_find_segment_index(ctx, address, &seg_idx)) return false;

    const RDSegmentFullVect* segments = rd_i_db_get_segments(ctx);
    const RDSegmentFull* seg = *vect_at(segments, seg_idx);

    usize idx = rd_i_address2index(seg, address);
    if(rd_flagsbuffer_has_tail(seg->flags, idx))
        rd_i_flagsbuffer_expand_range(seg->flags, &idx, NULL);

    RDAddress head = seg->base.start_address + idx;

    // a jump is THE history event; repaints and scrolls are not
    rd_i_surfacestate_push_history(&self->state, &self->state.back_history);
    vect_clear(&self->state.fwd_history);

    int row = _rd_surface_find_row(self, head);

    if(row != -1) {
        // destination already visible: keep the top unchanged, move the
        // cursor, repeat a render cycle so highlights follow it
        rd_surface_set_pos(self, row, 0);
        return _rd_surface_render_at(self, self->state.start,
                                     self->state.start_sub_line);
    }

    // destination off-screen: center it by stepping back half a viewport
    // of ROWS from its head (row-granular: multi-row heads, banners and
    // hex chunks all count as the rows they render as)
    usize sub_line = 0;
    usize steps = self->max_rows / 2;

    for(usize i = 0; i < steps; i++) {
        if(!rd_i_row_step_back(ctx, &seg, &seg_idx, &idx, &sub_line)) break;
    }

    if(!_rd_surface_render_from(self, seg, seg_idx, idx, sub_line))
        return false;

    // bottom clamp: near the end of the listing, a centered top renders
    // fewer than max_rows rows and leaves the viewport half empty.
    // Pull the top up by the deficit so the viewport stays full and the
    // destination simply sits below middle.
    // (The top clamp is implicit: the step-back loop above breaks at the
    // database start, leaving the destination above middle).
    // If the whole listing is shorter than the viewport, this walks back to the
    // very beginning and stops.
    usize got = vect_length(&self->renderer->rows_front);

    if(got < self->max_rows) {
        usize deficit = self->max_rows - got;
        bool moved = false;

        for(usize i = 0; i < deficit; i++) {
            if(!rd_i_row_step_back(ctx, &seg, &seg_idx, &idx, &sub_line)) break;
            moved = true;
        }

        if(moved && !_rd_surface_render_from(self, seg, seg_idx, idx, sub_line))
            return false;
    }

    row = _rd_surface_find_row(self, head);
    rd_surface_set_pos(self, row != -1 ? row : 0, 0);

    // second cycle: finalize highlights from state.pos, which was only
    // known after the first render materialized the rows
    return _rd_surface_render_from(self, seg, seg_idx, idx, sub_line);
}

bool rd_surface_get_first_address(const RDSurface* self, RDAddress* address) {
    if(vect_is_empty(&self->renderer->rows_front)) return false;

    if(address) *address = vect_first(&self->renderer->rows_front)->address;
    return true;
}

int rd_surface_index_of(const RDSurface* self, RDAddress address) {
    return rd_i_renderer_index_of(self->renderer, address);
}

int rd_surface_last_index_of(const RDSurface* self, RDAddress address) {
    return rd_i_renderer_last_index_of(self->renderer, address);
}

usize rd_surface_get_row_count(const RDSurface* self) {
    return rd_i_renderer_get_row_count(self->renderer);
}

usize rd_surface_get_byte_span(const RDSurface* self) {
    RDContext* ctx = self->renderer->context;
    const RDRowVect* rows = &self->renderer->rows_front;

    usize span = 0;
    const RDSegmentFull* run_seg = NULL;
    RDAddress run_start = 0;
    const RDRow* run_last = NULL;

    const RDRow* row;
    vect_each(row, rows) {
        const RDSegmentFull* seg = rd_i_db_find_segment(ctx, row->address);
        if(!seg) continue;

        if(seg != run_seg) { // crossed into a new segment: close the run
            if(run_last) {
                span +=
                    (run_last->address - run_start) + run_last->bytes_length;
            }

            run_seg = seg;
            run_start = row->address;
        }

        run_last = row;
    }

    if(run_last)
        span += (run_last->address - run_start) + run_last->bytes_length;
    return span;
}

const char* rd_surface_get_selected_text(RDSurface* self) {
    RDSurfacePos startpos = rd_i_surfacestate_get_start_selection(&self->state);
    RDSurfacePos endpos = rd_i_surfacestate_get_end_selection(&self->state);
    return rd_i_renderer_get_text(self->renderer, startpos, endpos);
}

bool rd_surface_get_current_address(const RDSurface* self, RDAddress* address) {
    return rd_i_renderer_get_address(self->renderer, self->state.pos, address);
}

RDRowSlice rd_surface_get_row(const RDSurface* self, usize idx) {
    return rd_i_renderer_get_row(self->renderer, idx);
}

RDSurfacePathSlice rd_surface_get_path(RDSurface* self) {
    const RDSurfacePathVect* path =
        rd_i_surfacepath_build(&self->path_builder, &self->renderer->rows_front,
                               self->renderer->context);

    return vect_to_slice(RDSurfacePathSlice, path);
}

bool rd_surface_select(RDSurface* self, int row, int col) {
    rd_i_renderer_fit(self->renderer, &row, &col);
    return rd_i_surfacestate_select(&self->state, row, col);
}

bool rd_surface_select_word(RDSurface* self, int row, int col) {
    RDSurfacePos startpos, endpos;
    if(!rd_i_renderer_select_word(self->renderer, row, col, &startpos, &endpos))
        return false;

    rd_surface_set_pos(self, startpos.row, startpos.col);
    return rd_surface_select(self, endpos.row, endpos.col);
}
