#include "row.h"

void rd_i_rowvect_destroy(RDRowVect* self) {
    RDRow* row;
    vect_each(row, self) {
        vect_destroy(&row->data);
        vect_destroy(&row->cells);
    }
}

void rd_i_rowvect_push(RDRowVect* self, LIndex index,
                       const RDListingItem* item) {
    RDRow r = {
        .index = index,
        .address = item->address,
        .curr_data = rd_i_default_cell_data(),
    };

    r.curr_data.is_instruction = item->kind == RD_LK_INSTRUCTION;

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
