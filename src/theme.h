#pragma once

#include <redasm/theme.h>
#include <stdbool.h>

typedef char RDColor[8]; // 7 chars + null terminator

typedef struct RDTheme {
    RDColor fg, bg, seek;
    RDColor comment, auto_comment;
    RDColor highlight_fg, highlight_bg;
    RDColor selection_fg, selection_bg;
    RDColor cursor_fg, cursor_bg;
    RDColor segment, function, type;
    RDColor address, constant, reg;
    RDColor string, symbol, data, pointer, imported;
    RDColor nop, ret, call, jump, jump_cond;
    RDColor entry_fg, entry_bg;
    RDColor graph_bg, graph_edge, graph_edge_loop, graph_edge_loop_cond;
    RDColor success, fail, warning;
} RDTheme;

void rd_i_theme_init(RDTheme* self);
bool rd_i_theme_set_color(RDTheme* self, RDThemeKind kind, const char* color);
