#include "state.h"
#include "support/containers.h"

static inline bool _rd_history_item_equals(const RDHistoryItem* a,
                                           const RDHistoryItem* b) {
    return a->start == b->start && a->pos.row == b->pos.row &&
           a->pos.col == b->pos.col;
}

void rd_i_surfacestate_push_history(RDSurfaceState* self,
                                    RDHistoryVect* history) {
    if(self->lock_history) return;

    RDHistoryItem item = {
        .start = self->start,
        .pos = self->pos,
    };

    if(vect_is_empty(history) ||
       !_rd_history_item_equals(vect_back(history), &item)) {
        vect_push(history, item);
    }
}

bool rd_i_surfacestate_can_go_back(const RDSurfaceState* self) {
    return !vect_is_empty(&self->back_history);
}

bool rd_i_surfacestate_can_go_forward(const RDSurfaceState* self) {
    return !vect_is_empty(&self->fwd_history);
}

bool rd_i_surfacestate_has_selection(const RDSurfaceState* self) {
    return (self->pos.row != self->sel_pos.row) ||
           (self->pos.col != self->sel_pos.col);
}

bool rd_i_surfacestate_go_back(RDSurfaceState* self) {
    if(vect_is_empty(&self->back_history)) return false;

    RDHistoryItem item = vect_pop_back(&self->back_history);
    rd_i_surfacestate_push_history(self, &self->fwd_history);

    rd_i_surfacestate_with_locked_history(self, {
        self->start = item.start;
        rd_i_surfacestate_set_pos(self, item.pos.row, item.pos.col);
    });

    return true;
}

bool rd_i_surfacestate_go_forward(RDSurfaceState* self) {
    if(vect_is_empty(&self->fwd_history)) return false;

    RDHistoryItem item = vect_pop_back(&self->fwd_history);
    rd_i_surfacestate_push_history(self, &self->back_history);

    rd_i_surfacestate_with_locked_history(self, {
        self->start = item.start;
        rd_i_surfacestate_set_pos(self, item.pos.row, item.pos.col);
    });

    return true;
}

bool rd_i_surfacestate_set_pos(RDSurfaceState* self, int row, int col) {
    if(row == self->sel_pos.row && col == self->sel_pos.col) return false;

    if(row >= 0) self->sel_pos.row = row;
    if(col >= 0) self->sel_pos.col = col;

    rd_i_surfacestate_select(self, row, col);
    return true;
}

bool rd_i_surfacestate_select(RDSurfaceState* self, int row, int col) {
    if(row == self->pos.row && col == self->pos.col) return false;

    if(row >= 0) self->pos.row = row;
    if(col >= 0) self->pos.col = col;

    return true;
}

RDSurfacePos rd_i_surfacestate_get_start_selection(const RDSurfaceState* self) {
    if(self->pos.row < self->sel_pos.row) return self->pos;

    if(self->pos.row == self->sel_pos.row) {
        if(self->pos.col < self->sel_pos.col) return self->pos;
    }

    return self->sel_pos;
}

RDSurfacePos rd_i_surfacestate_get_end_selection(const RDSurfaceState* self) {
    if(self->pos.row > self->sel_pos.row) return self->pos;

    if(self->pos.row == self->sel_pos.row) {
        if(self->pos.col > self->sel_pos.col) return self->pos;
    }

    return self->sel_pos;
}

void rd_i_surfacestate_clear_selection(RDSurfaceState* self) {
    self->sel_pos = self->pos;
}

void rd_i_surfacestate_clear_history(RDSurfaceState* self) {
    vect_clear(&self->back_history);
    vect_clear(&self->fwd_history);
}

void rd_i_surfacestate_deinit(RDSurfaceState* self) {
    vect_destroy(&self->fwd_history);
    vect_destroy(&self->back_history);
}
