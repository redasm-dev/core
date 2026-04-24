#pragma once

#include "core/db/db.h"
#include "listing/listing.h"
#include "support/utils.h"
#include <redasm/config.h>
#include <redasm/listing.h>
#include <redasm/surface/common.h>
#include <redasm/surface/renderer.h>

typedef struct RDCellVect {
    RDCell* data;
    usize length; // padded length
    usize capacity;

    LIndex index;
    RDAddress address;
    usize content_length; // length before padding (number of valid cells)
} RDCellVect;

typedef struct RDRowVect {
    RDCellVect* data;
    usize length;
    usize capacity;
} RDRowVect;

typedef struct RDRenderer {
    RDContext* context;
    const RDSegmentFull* curr_segment;
    usize flags, columns, auto_columns;
    RDAddress curr_address;
    LIndex listing_idx;
    RDRowVect rows_front, rows_back;
    RDCharVect comment_buf;
    RDCharVect word_buf;
    RDCharVect text_buf;
    RDXRefVect xrefs;
    char* hl_word;
    bool has_prev_mnem;
} RDRenderer;

static inline bool rd_i_renderer_has_flag(RDRenderer* self, usize f) {
    return self->flags & f;
}

RDRenderer* rd_i_renderer_create(RDContext* ctx, usize flags);
void rd_i_renderer_destroy(RDRenderer* self);
void rd_i_renderer_clear(RDRenderer* self);
void rd_i_renderer_swap(RDRenderer* self);
void rd_i_renderer_set_rdil(RDRenderer* self, bool b);
void rd_i_renderer_set_cursor_visible(RDRenderer* self, bool b);
void rd_i_renderer_fill_columns(RDRenderer* self);
void rd_i_renderer_highlight_row(RDRenderer* self, int row);
void rd_i_renderer_highlight_cursor(RDRenderer* self, int row, int col);
void rd_i_renderer_highlight_words(RDRenderer* self, int row, int col);
void rd_i_renderer_highlight_selection(RDRenderer* self, int startrow,
                                       int startcol, int endrow, int endcol);
void rd_i_renderer_new_row(RDRenderer* self, const RDListingItem* item);
void rd_i_renderer_rdil(RDRenderer* self, const RDListingItem* item);
void rd_i_renderer_instr(RDRenderer* self, const RDListingItem* item);
void rd_i_renderer_write_text(RDRenderer* self, RDCharVect* v);
const RDSegmentFull* rd_i_renderer_check_segment(RDRenderer* self,
                                                 const RDListingItem* item);
void rd_i_renderer_set_current_item(RDRenderer* self, LIndex idx,
                                    const RDListingItem* item);

const char* rd_i_renderer_get_text(RDRenderer* self, RDSurfacePos startpos,
                                   RDSurfacePos endpos);
int rd_i_renderer_index_of(const RDRenderer* self, RDAddress address);
int rd_i_renderer_last_index_of(const RDRenderer* self, RDAddress address);
void rd_i_renderer_set_highlight_word(RDRenderer* self, const char* w);
void rd_i_renderer_fit(const RDRenderer* self, int* row, int* col);
bool rd_i_renderer_is_index_visible(const RDRenderer* self, LIndex index);
const char* rd_i_renderer_get_word_under_pos(RDRenderer* self,
                                             const RDSurfacePos* pos);
bool rd_i_renderer_get_address_under_pos(RDRenderer* self,
                                         const RDSurfacePos* pos,
                                         RDAddress* address);
bool rd_i_renderer_get_address(const RDRenderer* self, RDSurfacePos pos,
                               RDAddress* address);
RDRowSlice rd_i_renderer_get_row(const RDRenderer* self, usize idx);
usize rd_i_renderer_get_row_count(const RDRenderer* self);
bool rd_i_renderer_select_word(RDRenderer* self, int row, int col,
                               RDSurfacePos* startpos, RDSurfacePos* endpos);
