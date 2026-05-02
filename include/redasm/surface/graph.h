#pragma once

#include <redasm/common.h>
#include <redasm/function.h>
#include <redasm/surface/common.h>

typedef struct RDSurfaceGraph RDSurfaceGraph;

// clang-format off
RD_API RDSurfaceGraph* rd_surfacegraph_create(RDContext* ctx, usize flags);
RD_API void rd_surfacegraph_destroy(RDSurfaceGraph* self);
RD_API void rd_surfacegraph_render(RDSurfaceGraph* self);
RD_API void rd_surfacegraph_clear_history(RDSurfaceGraph* self);
RD_API void rd_surfacegraph_clear_selection(RDSurfaceGraph* self);
RD_API void rd_surfacegraph_set_mode(RDSurfaceGraph* self, RDRenderMode m);
RD_API void rd_surfacegraph_set_cursor_visible(RDSurfaceGraph* self, bool b);
RD_API void rd_surfacegraph_set_highlight_word(RDSurfaceGraph* self, const char* word);
RD_API bool rd_surfacegraph_jump_to_ep(RDSurfaceGraph* self);
RD_API bool rd_surfacegraph_jump_to(RDSurfaceGraph* self, RDAddress address);
RD_API bool rd_surfacegraph_set_pos(RDSurfaceGraph* self, int row, int col);
RD_API bool rd_surfacegraph_select(RDSurfaceGraph* self, int row, int col);
RD_API bool rd_surfacegraph_select_word(RDSurfaceGraph* self, int row, int col);
RD_API bool rd_surfacegraph_go_back(RDSurfaceGraph* self);
RD_API bool rd_surfacegraph_go_forward(RDSurfaceGraph* self);
RD_API bool rd_surfacegraph_can_go_back(const RDSurfaceGraph* self);
RD_API bool rd_surfacegraph_can_go_forward(const RDSurfaceGraph* self);
RD_API bool rd_surfacegraph_has_selection(const RDSurfaceGraph* self);
RD_API bool rd_surfacegraph_get_current_address(const RDSurfaceGraph* self, RDAddress* address);
RD_API bool rd_surfacegraph_get_address_under_cursor(RDSurfaceGraph* self, RDAddress* address);
RD_API bool rd_surfacegraph_get_cell_data_under_cursor(const RDSurfaceGraph* self, RDCellData* cd);
RD_API bool rd_surfacegraph_get_address_under_pos(RDSurfaceGraph* self, const RDSurfacePos* pos, RDAddress* address);
RD_API bool rd_surfacegraph_get_cell_data_under_pos(const RDSurfaceGraph* self, const RDSurfacePos* pos, RDCellData* cd);
RD_API RDRenderMode rd_surfacegraph_get_mode(const RDSurfaceGraph* self);
RD_API int rd_surfacegraph_index_of(const RDSurfaceGraph* self, RDAddress address);
RD_API int rd_surfacegraph_last_index_of(const RDSurfaceGraph* self, RDAddress address);
RD_API usize rd_surfacegraph_get_row_count(const RDSurfaceGraph* self);
RD_API RDSurfacePos rd_surfacegraph_get_pos(const RDSurfaceGraph* self);
RD_API RDRowSlice rd_surfacegraph_get_row(const RDSurfaceGraph* self, usize idx);
RD_API const char* rd_surfacegraph_get_selected_text(RDSurfaceGraph* self);
RD_API const char* rd_surfacegraph_get_word_under_pos(const RDSurfaceGraph* self, const RDSurfacePos* pos);
RD_API const RDFunction* rd_surfacegraph_get_function(const RDSurfaceGraph* self);
RD_API RDGraph* rd_surfacegraph_get_graph(const RDSurfaceGraph* self);
// clang-format on
