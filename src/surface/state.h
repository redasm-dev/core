#pragma once

#include "listing/listing.h"
#include <redasm/surface/common.h>

typedef struct RDHistoryItem {
    LIndex start;
    RDSurfacePos pos;
} RDHistoryItem;

typedef struct RDHistoryVect {
    RDHistoryItem* data;
    usize length;
    usize capacity;
} RDHistoryVect;

typedef struct RDSurfaceState {
    RDSurfacePos pos, sel_pos;
    LIndex start;
    RDHistoryVect back_history, fwd_history;
    bool lock_history;
} RDSurfaceState;

#define rd_i_surfacestate_with_locked_history(self, ...)                       \
    do {                                                                       \
        (self)->lock_history = true;                                           \
        __VA_ARGS__;                                                           \
        (self)->lock_history = false;                                          \
    } while(0)

void rd_i_surfacestate_push_history(RDSurfaceState* self,
                                    RDHistoryVect* history);
bool rd_i_surfacestate_can_go_back(const RDSurfaceState* self);
bool rd_i_surfacestate_can_go_forward(const RDSurfaceState* self);
bool rd_i_surfacestate_go_back(RDSurfaceState* self);
bool rd_i_surfacestate_go_forward(RDSurfaceState* self);
bool rd_i_surfacestate_has_selection(const RDSurfaceState* self);
bool rd_i_surfacestate_set_pos(RDSurfaceState* self, int row, int col);
bool rd_i_surfacestate_select(RDSurfaceState* self, int row, int col);
RDSurfacePos rd_i_surfacestate_get_start_selection(const RDSurfaceState* self);
RDSurfacePos rd_i_surfacestate_get_end_selection(const RDSurfaceState* self);
void rd_i_surfacestate_clear_selection(RDSurfaceState* self);
void rd_i_surfacestate_clear_history(RDSurfaceState* self);
void rd_i_surfacestate_deinit(RDSurfaceState* self);
