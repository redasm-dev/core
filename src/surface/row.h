#pragma once

#include "core/segment.h"
#include "support/containers.h"
#include <assert.h>
#include <redasm/surface/common.h>

#define RD_SUB_LINE_INSTRUCTION 2
#define RD_SUB_LINE_NONE SIZE_MAX

#define RD_SURFACE_HEX_LINE 0x10

typedef enum {
    RD_ROW_OK = 0,    // rendered a row at (idx, sub_line)
    RD_ROW_EXHAUSTED, // no such sub_line for this idx, advance to next address
} RDRowStatus;

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

    RDAddress address;
    usize sub_line;

    usize bytes_length;
    int content_length; // length before padding (number of valid cells)

    RDCellData curr_data;
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
void rd_i_rowvect_push(RDContext* ctx, RDRowVect* self, usize sub_line,
                       RDAddress address);

void rd_i_row_reserve(RDRow* self, int n);
void rd_i_row_push(RDRow* self, u32 ch, RDThemeKind fg, RDThemeKind bg);
usize rd_i_row_code_instr_sub_line(const RDSegmentFull* seg, usize idx);

static inline RDCell* rd_i_row_cell_at(RDRow* self, int idx) {
    return vect_at(&self->cells, idx);
}

static inline RDCellData* rd_i_row_data_at(RDRow* self, int idx) {
    return vect_at(&self->data, idx);
}

static inline usize rd_i_row_is_empty(const RDRow* self) {
    assert(vect_length(&self->cells) == (vect_length(&self->data)));
    return vect_is_empty(&self->cells);
}

static inline int rd_i_row_length(const RDRow* self) {
    assert(vect_length(&self->cells) == (vect_length(&self->data)));
    return (int)vect_length(&self->cells);
}

bool rd_i_row_step_back(RDContext* ctx, const RDSegmentFull** seg,
                        usize* seg_idx, usize* idx, usize* sub_line);
