#include "core/context.h"
#include "listing/listing.h"
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

static void _rd_surface_render_range(RDSurface* self, LIndex idx, usize n) {
    const RDListing* listing = &self->renderer->context->listing;

    for(usize i = 0; idx < vect_length(listing) && i < n; idx++, i++)
        rd_i_render_item_at(self->renderer, idx);
}

RDSurface* rd_surface_create(RDContext* ctx, RDRenderFlags flags) {
    RDSurface* self = malloc(sizeof(*self));

    *self = (RDSurface){
        .renderer = rd_i_renderer_create(ctx, flags),
    };

    rd_i_surfacestate_with_locked_history(&self->state,
                                          rd_surface_jump_to_ep(self));
    return self;
}

void rd_surface_destroy(RDSurface* self) {
    if(!self) return;
    rd_i_surfacepath_deinit(&self->path_builder);
    rd_i_surfacestate_deinit(&self->state);
    rd_i_renderer_destroy(self->renderer);
    free(self);
}

void rd_surface_seek(RDSurface* self, usize index) {
    self->state.start = index;
}

void rd_surface_set_highlight_word(RDSurface* self, const char* word) {
    rd_i_renderer_set_highlight_word(self->renderer, word);
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

void rd_surface_set_columns(RDSurface* self, usize cols) {
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
    return rd_i_surfacestate_go_back(&self->state);
}

bool rd_surface_go_forward(RDSurface* self) {
    return rd_i_surfacestate_go_forward(&self->state);
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

void rd_surface_render(RDSurface* self, usize n) {
    rd_i_renderer_clear(self->renderer);
    _rd_surface_render_range(self, self->state.start, n);
    _rd_surface_render_finalize(self);
}

usize rd_surface_get_row_start(const RDSurface* self) {
    return self->state.start;
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

usize rd_surface_get_length(const RDSurface* self) {
    return vect_length(&self->renderer->context->listing);
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
    const RDSurfacePathVect* path = rd_i_surfacepath_build(
        &self->path_builder, self->state.start, &self->renderer->rows_front,
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

bool rd_surface_jump_to_ep(RDSurface* self) {
    bool res = false;

    rd_i_surfacestate_with_locked_history(&self->state, {
        const RDContext* ctx = self->renderer->context;

        RDAddress ep;
        res = rd_get_entry_point(ctx, &ep);
        if(res) res = rd_surface_jump_to(self, ep);
    });

    return res;
}

bool rd_surface_jump_to(RDSurface* self, RDAddress address) {
    const RDContext* ctx = self->renderer->context;
    LIndex index = rd_i_listing_lower_bound(&ctx->listing, address);
    if(index == vect_length(&ctx->listing)) return false;

    rd_i_surfacestate_push_history(&self->state, &self->state.back_history);
    vect_clear(&self->state.fwd_history);

    if(!rd_i_renderer_is_index_visible(self->renderer, index)) {
        const usize DIFF = (vect_length(&self->renderer->rows_front) / 4);
        usize rindex = index;
        if(rindex > DIFF) rindex -= DIFF;

        self->state.start = rindex;
        rd_surface_set_pos(self, index - rindex, -1);
    }
    else
        rd_surface_set_pos(self, index - self->state.start, -1);

    return true;
}
