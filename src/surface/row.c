#include "row.h"

void rd_i_rowvect_destroy(RDRowVect* self) {
    RDRow* row;
    vect_each(row, self) {
        vect_destroy(&row->meta);
        vect_destroy(&row->cells);
    }
}

void rd_i_rowvect_push(RDRowVect* self, LIndex index, RDAddress address) {
    vect_push(self, (RDRow){
                        .index = index,
                        .address = address,
                        .curr_meta = rd_i_default_cell_meta(),
                    });
}

void rd_i_row_reserve(RDRow* self, usize n) {
    vect_reserve(&self->cells, n);
    vect_reserve(&self->meta, n);
}

void rd_i_row_push(RDRow* self, char ch, RDThemeKind fg, RDThemeKind bg) {
    assert(vect_length(&self->cells) == (vect_length(&self->meta)));

    if(fg == RD_THEME_DEFAULT) fg = RD_THEME_FOREGROUND;
    if(bg == RD_THEME_DEFAULT) bg = RD_THEME_BACKGROUND;
    vect_push(&self->cells, (RDCell){.ch = ch, .fg = fg, .bg = bg});
    vect_push(&self->meta, self->curr_meta);
}
