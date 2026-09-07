#include "row.h"
#include "core/context.h"
#include "support/error.h"
#include <inttypes.h>

void rd_i_rowvect_destroy(RDRowVect* self) {
    RDRow* row;
    vect_each(row, self) {
        vect_destroy(&row->data);
        vect_destroy(&row->cells);
    }
}

void rd_i_rowvect_push(RDRowVect* self, usize sub_line, RDAddress address) {
    RDRow r = {
        .address = address,
        .sub_line = sub_line,
        .curr_data = rd_i_default_cell_data(),
    };

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

bool rd_i_row_step_back(RDContext* ctx, RDRenderFlags flags,
                        RDRowDescVect* scratch, const RDSegmentFull** seg,
                        usize* seg_idx, usize* idx, usize* sub_line) {
    const RDSegmentFullVect* segments = rd_i_db_get_segments(ctx);

    // (1) not at the item's first row: just go shallower.
    if(*sub_line > 0) {
        (*sub_line)--;
        return true;
    }

    // (2) cross to the previous head.
    if(*idx == 0) {
        if(*seg_idx == 0) return false;

        (*seg_idx)--;
        *seg = *vect_at(segments, *seg_idx);
        *idx = rd_flagsbuffer_get_length((*seg)->flags);
    }

    (*idx)--;

    if(rd_flagsbuffer_has_unknown((*seg)->flags, *idx)) {
        while(*idx > 0 && !rd_i_is_hexchunk_head(*seg, *idx))
            (*idx)--;
    }
    else if(rd_flagsbuffer_has_tail((*seg)->flags, *idx)) {
        while(*idx > 0 && rd_flagsbuffer_has_tail((*seg)->flags, *idx))
            (*idx)--;

        panic_if(rd_flagsbuffer_has_tail((*seg)->flags, *idx),
                 "item spans segment boundary");
    }

    // (3) land on that head's last row.
    rd_i_item_layout(ctx, flags, *seg, *idx, scratch);
    *sub_line = vect_length(scratch) - 1;
    return true;
}
