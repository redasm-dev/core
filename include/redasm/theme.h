#pragma once

#include <redasm/config.h>

// clang-format off
typedef enum {
    RD_THEME_DEFAULT = 0,

    // surface fundamentals
    RD_THEME_FOREGROUND, RD_THEME_BACKGROUND, RD_THEME_SEEK,

    // surface interactions
    RD_THEME_HIGHLIGHT_FG, RD_THEME_HIGHLIGHT_BG,
    RD_THEME_SELECTION_FG, RD_THEME_SELECTION_BG,
    RD_THEME_CURSOR_FG, RD_THEME_CURSOR_BG,

    // structural listing items
    RD_THEME_SEGMENT, RD_THEME_FUNCTION, RD_THEME_TYPE,

    // tokens
    RD_THEME_LOCATION, RD_THEME_NUMBER, RD_THEME_REG, RD_THEME_STRING,

    // annotations
    RD_THEME_COMMENT,

    // instruction semantics
    RD_THEME_STOP, RD_THEME_JUMP, RD_THEME_JUMP_COND, RD_THEME_CALL, RD_THEME_CALL_COND,

    // diagnostic / severity
    RD_THEME_SUCCESS, RD_THEME_FAIL, RD_THEME_WARNING, RD_THEME_MUTED,

    // flags surface mode
    RD_THEME_FLAG_CODE, RD_THEME_FLAG_DATA,

    RD_THEME_COUNT,
} RDThemeKind;
// clang-format on

RD_API bool rd_set_theme_color(RDThemeKind kind, const char* color);
RD_API const char* rd_get_theme_color(RDThemeKind kind);
