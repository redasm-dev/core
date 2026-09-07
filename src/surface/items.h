#pragma once

#include "surface/renderer.h"

void rd_i_render_row(RDRenderer* r, const RDSegmentFull* seg, usize idx,
                     usize sub_line, const RDRowDesc* d);
void rd_i_render_item(RDRenderer* r, const RDSegmentFull* seg, usize idx,
                      usize sub_line);
void rd_i_render_item_any(RDRenderer* r, const RDSegmentFull* seg, usize idx);
