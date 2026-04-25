#pragma once

#include <redasm/function.h>
#include <redasm/listing.h>
#include <redasm/segment.h>
#include <redasm/surface/common.h>

typedef struct RDSurface RDSurface;

typedef struct RDSurfacePathItem {
    int from_row; // = -1 if out of range
    int to_row;   // = rows + 1 if out of range
    RDThemeKind style;
    bool is_loop;
} RDSurfacePathItem;

typedef struct RDSurfacePathSlice {
    const RDSurfacePathItem* data;
    usize length;
} RDSurfacePathSlice;

// clang-format off
RD_API RDSurface* rd_surface_create(RDContext* ctx, RDRenderFlags flags);
RD_API void rd_surface_destroy(RDSurface* self);
RD_API void rd_surface_clear_history(RDSurface* self);
RD_API void rd_surface_clear_selection(RDSurface* self);
RD_API void rd_surface_render(RDSurface* self, usize n);
RD_API void rd_surface_set_mode(RDSurface* self, RDRenderMode m);
RD_API void rd_surface_set_cursor_visible(RDSurface* self, bool b);
RD_API void rd_surface_set_columns(RDSurface* self, usize cols);
RD_API void rd_surface_seek(RDSurface* self, usize index);
RD_API void rd_surface_set_highlight_word(RDSurface* self, const char* word);
RD_API RDRenderMode rd_surface_get_mode(const RDSurface* self);
RD_API bool rd_surface_has_selection(const RDSurface* self);
RD_API bool rd_surface_can_go_back(const RDSurface* self);
RD_API bool rd_surface_can_go_forward(const RDSurface* self);
RD_API bool rd_surface_set_pos(RDSurface* self, int row, int col);
RD_API bool rd_surface_select(RDSurface* self, int row, int col);
RD_API bool rd_surface_select_word(RDSurface* self, int row, int col);
RD_API bool rd_surface_go_back(RDSurface* self);
RD_API bool rd_surface_go_forward(RDSurface* self);
RD_API bool rd_surface_jump_to_ep(RDSurface* self);
RD_API bool rd_surface_jump_to(RDSurface* self, RDAddress address);
RD_API bool rd_surface_get_current_address(const RDSurface* self, RDAddress* address);
RD_API bool rd_surface_get_address_under_pos(const RDSurface* self, const RDSurfacePos* pos, RDAddress* address);
RD_API bool rd_surface_get_address_under_cursor(const RDSurface* self, RDAddress* address);
RD_API int rd_surface_index_of(const RDSurface* self, RDAddress address);
RD_API int rd_surface_last_index_of(const RDSurface* self, RDAddress address);
RD_API usize rd_surface_get_row_start(const RDSurface* self);
RD_API usize rd_surface_get_row_count(const RDSurface* self);
RD_API usize rd_surface_get_length(const RDSurface* self);
RD_API RDRowSlice rd_surface_get_row(const RDSurface* self, usize idx);
RD_API RDSurfacePathSlice rd_surface_get_path(RDSurface* self);
RD_API RDSurfacePos rd_surface_get_pos(const RDSurface* self);
RD_API const char* rd_surface_get_selected_text(RDSurface* self);
RD_API const char* rd_surface_get_word_under_pos(const RDSurface* self, const RDSurfacePos* pos);
// clang-format on
