#include "graphs/graph.h"
#include "core/context.h"
#include "redasm/context.h"
#include "support/containers.h"
#include "surface/items.h"
#include "surface/renderer.h"
#include "surface/state.h"
#include <redasm/surface/graph.h>
#include <stdlib.h>

typedef struct RDSurfaceGraph {
    RDRenderer* renderer;
    RDSurfaceState state;
    const RDFunction* function;
} RDSurfaceGraph;

static void _rd_surfacegraph_render_finalize(RDSurfaceGraph* self) {
    rd_i_renderer_fill_columns(self->renderer);
    rd_i_renderer_highlight_row(self->renderer, self->state.pos.row);

    if(!rd_surfacegraph_has_selection(self)) {
        rd_i_renderer_highlight_words(self->renderer, self->state.pos.row,
                                      self->state.pos.col);
    }

    RDSurfacePos start = rd_i_surfacestate_get_start_selection(&self->state);
    RDSurfacePos end = rd_i_surfacestate_get_end_selection(&self->state);

    rd_i_renderer_highlight_selection(self->renderer, start.row, start.col,
                                      end.row, end.col);
    rd_i_renderer_highlight_cursor(self->renderer, self->state.pos.row,
                                   self->state.pos.col);
    rd_i_renderer_swap(self->renderer);
}

RDSurfaceGraph* rd_surfacegraph_create(RDContext* ctx, usize flags) {
    RDSurfaceGraph* self = malloc(sizeof(*self));
    *self = (RDSurfaceGraph){.renderer = rd_i_renderer_create(ctx, flags)};
    return self;
}

void rd_surfacegraph_destroy(RDSurfaceGraph* self) {
    if(!self) return;
    rd_i_surfacestate_deinit(&self->state);
    rd_i_renderer_destroy(self->renderer);
    free(self);
}

void rd_surfacegraph_render(RDSurfaceGraph* self) {
    rd_i_renderer_clear(self->renderer);
    if(!self->function || !self->function->graph) return;

    const RDContext* ctx = self->renderer->context;
    const RDListing* listing = &ctx->listing;
    const RDNodeVect* nodes = rd_i_graph_get_nodes(self->function->graph);

    RDFunctionChunkVect chunks = {0};
    vect_reserve(&chunks, vect_length(nodes));

    const RDGraphNode* n;
    vect_each(n, nodes) {
        RDFunctionChunk* chunk = rd_i_function_get_chunk(self->function, *n);
        assert(chunk && "node without chunk");
        vect_push(&chunks, chunk);
    }

    rd_i_functionchunk_sort(&chunks);

    RDFunctionChunk** chunk;
    vect_each(chunk, &chunks) {
        LIndex idx = rd_i_listing_lower_bound(listing, (*chunk)->start);
        for(; idx < vect_length(listing); idx++) {
            const RDListingItem* item = vect_at(listing, idx);
            if(item->address >= (*chunk)->end) break;
            rd_i_render_item_at(self->renderer, idx);
        }
    }

    vect_destroy(&chunks);
    _rd_surfacegraph_render_finalize(self);
}

bool rd_surfacegraph_jump_to_ep(RDSurfaceGraph* self) {
    bool res = false;

    rd_i_surfacestate_with_locked_history(&self->state, {
        const RDContext* ctx = self->renderer->context;

        RDAddress ep;
        res = rd_get_entry_point(ctx, &ep);
        if(res) res = rd_surfacegraph_jump_to(self, ep);
    });

    return res;
}

bool rd_surfacegraph_jump_to(RDSurfaceGraph* self, RDAddress address) {
    RDContext* ctx = self->renderer->context;
    const RDFunction* f = rd_find_function(ctx, address);
    if(!f) return false;

    if(f != self->function) {
        self->function = f;

        // anchor start to function entry
        LIndex startidx = rd_i_listing_lower_bound(&ctx->listing, f->address);
        if(startidx == vect_length(&ctx->listing)) return false;

        rd_i_surfacestate_push_history(&self->state, &self->state.back_history);
        vect_clear(&self->state.fwd_history);
        self->state.start = startidx;
    }

    // render then set pos: rows_front must be populated first
    rd_surfacegraph_render(self);

    int row = rd_i_renderer_index_of(self->renderer, address);
    rd_surfacegraph_set_pos(self, row != -1 ? row : 0, 0);
    return true;
}

bool rd_surfacegraph_set_pos(RDSurfaceGraph* self, int row, int col) {
    rd_i_renderer_fit(self->renderer, &row, &col);
    return rd_i_surfacestate_set_pos(&self->state, row, col);
}

bool rd_surfacegraph_select(RDSurfaceGraph* self, int row, int col) {
    rd_i_renderer_fit(self->renderer, &row, &col);
    return rd_i_surfacestate_select(&self->state, row, col);
}

bool rd_surfacegraph_select_word(RDSurfaceGraph* self, int row, int col) {
    RDSurfacePos startpos, endpos;
    if(!rd_i_renderer_select_word(self->renderer, row, col, &startpos, &endpos))
        return false;

    rd_surfacegraph_set_pos(self, startpos.row, startpos.col);
    return rd_surfacegraph_select(self, endpos.row, endpos.col);
}

bool rd_surfacegraph_go_back(RDSurfaceGraph* self) {
    if(!rd_i_surfacestate_go_back(&self->state)) return false;

    const RDContext* ctx = self->renderer->context;
    const RDListing* listing = &ctx->listing;

    // use start to find the function.
    // start is the listing index of function
    if(self->state.start < vect_length(listing)) {
        RDAddress addr = vect_at(listing, self->state.start)->address;
        self->function = rd_find_function(ctx, addr);
    }

    rd_surfacegraph_render(self);
    return true;
}

bool rd_surfacegraph_go_forward(RDSurfaceGraph* self) {
    if(!rd_i_surfacestate_go_forward(&self->state)) return false;

    const RDContext* ctx = self->renderer->context;
    const RDListing* listing = &ctx->listing;

    // use start to find the function.
    // start is the listing index of function
    if(self->state.start < vect_length(listing)) {
        RDAddress addr = vect_at(listing, self->state.start)->address;
        self->function = rd_find_function(ctx, addr);
    }

    rd_surfacegraph_render(self);
    return true;
}

bool rd_surfacegraph_can_go_back(const RDSurfaceGraph* self) {
    return rd_i_surfacestate_can_go_back(&self->state);
}

bool rd_surfacegraph_can_go_forward(const RDSurfaceGraph* self) {
    return rd_i_surfacestate_can_go_forward(&self->state);
}

void rd_surfacegraph_clear_history(RDSurfaceGraph* self) {
    rd_i_surfacestate_clear_history(&self->state);
}

bool rd_surfacegraph_has_selection(const RDSurfaceGraph* self) {
    return rd_i_surfacestate_has_selection(&self->state);
}

void rd_surfacegraph_clear_selection(RDSurfaceGraph* self) {
    rd_i_surfacestate_clear_selection(&self->state);
}

RDSurfacePos rd_surfacegraph_get_pos(const RDSurfaceGraph* self) {
    return self->state.pos;
}

bool rd_surfacegraph_get_current_address(const RDSurfaceGraph* self,
                                         RDAddress* address) {
    return rd_i_renderer_get_address(self->renderer, self->state.pos, address);
}

const char* rd_surfacegraph_get_word_under_pos(const RDSurfaceGraph* self,
                                               const RDSurfacePos* pos) {
    return rd_i_renderer_get_word_under_pos(self->renderer, pos);
}

bool rd_surfacegraph_get_address_under_cursor(RDSurfaceGraph* self,
                                              RDAddress* address) {
    return rd_i_renderer_get_address_under_pos(self->renderer, &self->state.pos,
                                               address);
}

bool rd_surfacegraph_get_address_under_pos(RDSurfaceGraph* self,
                                           const RDSurfacePos* pos,
                                           RDAddress* address) {
    return rd_i_renderer_get_address_under_pos(self->renderer, pos, address);
}

int rd_surfacegraph_index_of(const RDSurfaceGraph* self, RDAddress address) {
    return rd_i_renderer_index_of(self->renderer, address);
}

int rd_surfacegraph_last_index_of(const RDSurfaceGraph* self,
                                  RDAddress address) {
    return rd_i_renderer_last_index_of(self->renderer, address);
}

usize rd_surfacegraph_get_row_count(const RDSurfaceGraph* self) {
    return rd_i_renderer_get_row_count(self->renderer);
}

RDRowSlice rd_surfacegraph_get_row(const RDSurfaceGraph* self, usize idx) {
    return rd_i_renderer_get_row(self->renderer, idx);
}

const char* rd_surfacegraph_get_selected_text(RDSurfaceGraph* self) {
    RDSurfacePos start = rd_i_surfacestate_get_start_selection(&self->state);
    RDSurfacePos end = rd_i_surfacestate_get_end_selection(&self->state);
    return rd_i_renderer_get_text(self->renderer, start, end);
}

bool rd_surfacegraph_has_rdil(const RDSurfaceGraph* self) {
    return rd_i_renderer_has_flag(self->renderer, RD_RENDERER_RDIL);
}

void rd_surfacegraph_set_rdil(RDSurfaceGraph* self, bool b) {
    rd_i_renderer_set_rdil(self->renderer, b);
}

void rd_surfacegraph_set_cursor_visible(RDSurfaceGraph* self, bool b) {
    rd_i_renderer_set_cursor_visible(self->renderer, b);
}

void rd_surfacegraph_set_highlight_word(RDSurfaceGraph* self,
                                        const char* word) {
    rd_i_renderer_set_highlight_word(self->renderer, word);
}

const RDFunction* rd_surfacegraph_get_function(const RDSurfaceGraph* self) {
    return self->function;
}

RDGraph* rd_surfacegraph_get_graph(const RDSurfaceGraph* self) {
    if(self->function) return self->function->graph;
    return NULL;
}
