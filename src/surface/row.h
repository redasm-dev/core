#pragma once

#include "listing/listing.h"
#include "support/containers.h"
#include <assert.h>
#include <redasm/surface/common.h>

typedef struct RDCellVect {
    RDCell* data;
    usize length; // padded length
    usize capacity;
} RDCellVect;

typedef struct RDCellDataVect {
    RDCellData* data;
    usize length;
    usize capacity;
} RDCellDataVect;

typedef struct RDRow {
    RDCellVect cells;
    RDCellDataVect data; // parallel array of 'cells'

    LIndex index;
    RDAddress address;
    usize content_length; // length before padding (number of valid cells)

    RDCellData curr_meta;
} RDRow;

typedef struct RDRowVect {
    RDRow* data;
    usize length;
    usize capacity;
} RDRowVect;

static inline RDCellData rd_i_default_cell_data(void) {
    return (RDCellData){
        .operand = {.index = -1},
    };
}

void rd_i_rowvect_destroy(RDRowVect* self);
void rd_i_rowvect_push(RDRowVect* self, LIndex index, RDAddress address);

void rd_i_row_reserve(RDRow* self, usize n);
void rd_i_row_push(RDRow* self, char ch, RDThemeKind fg, RDThemeKind bg);

static inline RDCell* rd_i_row_cell_at(RDRow* self, usize idx) {
    return vect_at(&self->cells, idx);
}

static inline RDCellData* rd_i_row_meta_at(RDRow* self, usize idx) {
    return vect_at(&self->data, idx);
}

static inline usize rd_i_row_is_empty(const RDRow* self) {
    assert(vect_length(&self->cells) == (vect_length(&self->data)));
    return vect_is_empty(&self->cells);
}

static inline usize rd_i_row_length(const RDRow* self) {
    assert(vect_length(&self->cells) == (vect_length(&self->data)));
    return vect_length(&self->cells);
}
