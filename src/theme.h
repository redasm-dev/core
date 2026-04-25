#pragma once

#include <redasm/theme.h>
#include <stdbool.h>

typedef char RDColor[8]; // 7 chars + null terminator

typedef struct RDTheme {
    RDColor fg, bg, seek;
    RDColor highlight_fg, highlight_bg;
    RDColor selection_fg, selection_bg;
    RDColor cursor_fg, cursor_bg;
    RDColor segment, function, type;
    RDColor location, number, reg, string;
    RDColor comment;
    RDColor stop, jump, jump_cond, call, call_cond;
    RDColor success, fail, warning, muted;
    RDColor flag_code, flag_data;
} RDTheme;

void rd_i_theme_init(RDTheme* self);
bool rd_i_theme_set_color(RDTheme* self, RDThemeKind kind, const char* color);
