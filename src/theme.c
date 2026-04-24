#include "theme.h"
#include "core/state.h"
#include "support/logging.h"
#include <ctype.h>
#include <stddef.h>
#include <string.h>

typedef struct RDThemeParam {
    size_t offset;
    RDThemeKind alt;
} RDThemeParam;

// clang-format off
static const RDThemeParam RD_THEME_PARAMS[] = {
    [RD_THEME_FOREGROUND]           = {offsetof(RDTheme, fg), 0},
    [RD_THEME_BACKGROUND]           = {offsetof(RDTheme, bg), 0},
    [RD_THEME_SEEK]                 = {offsetof(RDTheme, seek), RD_THEME_BACKGROUND},
    [RD_THEME_COMMENT]              = {offsetof(RDTheme, comment), RD_THEME_FOREGROUND},
    [RD_THEME_AUTOCOMMENT]          = {offsetof(RDTheme, auto_comment), RD_THEME_FOREGROUND},
    [RD_THEME_HIGHLIGHT_FG]         = {offsetof(RDTheme, highlight_fg), RD_THEME_BACKGROUND},
    [RD_THEME_HIGHLIGHT_BG]         = {offsetof(RDTheme, highlight_bg), RD_THEME_FOREGROUND},
    [RD_THEME_SELECTION_FG]         = {offsetof(RDTheme, selection_fg), RD_THEME_BACKGROUND},
    [RD_THEME_SELECTION_BG]         = {offsetof(RDTheme, selection_bg), RD_THEME_FOREGROUND},
    [RD_THEME_CURSOR_FG]            = {offsetof(RDTheme, cursor_fg), RD_THEME_BACKGROUND},
    [RD_THEME_CURSOR_BG]            = {offsetof(RDTheme, cursor_bg), RD_THEME_FOREGROUND},
    [RD_THEME_SEGMENT]              = {offsetof(RDTheme, segment), RD_THEME_FOREGROUND},
    [RD_THEME_FUNCTION]             = {offsetof(RDTheme, function), RD_THEME_FOREGROUND},
    [RD_THEME_TYPE]                 = {offsetof(RDTheme, type), RD_THEME_FOREGROUND},
    [RD_THEME_ADDRESS]              = {offsetof(RDTheme, address), RD_THEME_FOREGROUND},
    [RD_THEME_CONSTANT]             = {offsetof(RDTheme, constant), RD_THEME_FOREGROUND},
    [RD_THEME_REG]                  = {offsetof(RDTheme, reg), RD_THEME_FOREGROUND},
    [RD_THEME_STRING]               = {offsetof(RDTheme, string), RD_THEME_FOREGROUND},
    [RD_THEME_LABEL]                = {offsetof(RDTheme, symbol), RD_THEME_FOREGROUND},
    [RD_THEME_DATA]                 = {offsetof(RDTheme, data), RD_THEME_FOREGROUND},
    [RD_THEME_POINTER]              = {offsetof(RDTheme, pointer), RD_THEME_FOREGROUND},
    [RD_THEME_IMPORT]               = {offsetof(RDTheme, imported), RD_THEME_FOREGROUND},
    [RD_THEME_NOP]                  = {offsetof(RDTheme, nop), RD_THEME_FOREGROUND},
    [RD_THEME_RET]                  = {offsetof(RDTheme, ret), RD_THEME_FOREGROUND},
    [RD_THEME_CALL]                 = {offsetof(RDTheme, call), RD_THEME_FOREGROUND},
    [RD_THEME_JUMP]                 = {offsetof(RDTheme, jump), RD_THEME_FOREGROUND},
    [RD_THEME_JUMP_COND]            = {offsetof(RDTheme, jump_cond), RD_THEME_FOREGROUND},
    [RD_THEME_ENTRY_FG]             = {offsetof(RDTheme, entry_fg), RD_THEME_FOREGROUND},
    [RD_THEME_ENTRY_BG]             = {offsetof(RDTheme, entry_bg), RD_THEME_BACKGROUND},
    [RD_THEME_GRAPH_BG]             = {offsetof(RDTheme, graph_bg), RD_THEME_BACKGROUND},
    [RD_THEME_GRAPH_EDGE]           = {offsetof(RDTheme, graph_edge), RD_THEME_FOREGROUND},
    [RD_THEME_GRAPH_EDGE_LOOP]      = {offsetof(RDTheme, graph_edge_loop), RD_THEME_FOREGROUND},
    [RD_THEME_GRAPH_EDGE_LOOP_COND] = {offsetof(RDTheme, graph_edge_loop_cond), RD_THEME_FOREGROUND},
    [RD_THEME_SUCCESS]              = {offsetof(RDTheme, success), RD_THEME_FOREGROUND},
    [RD_THEME_FAIL]                 = {offsetof(RDTheme, fail), RD_THEME_FOREGROUND},
    [RD_THEME_WARNING]              = {offsetof(RDTheme, warning), RD_THEME_FOREGROUND},
};
// clang-format on

static bool _rd_theme_validate_color(const char* color) {
    if(!color) {
        LOG_FAIL("color is NULL");
        return false;
    }

    if(strlen(color) != sizeof(RDColor) - 1) {
        LOG_FAIL("invalid color length: expected %zu, got %zu",
                 sizeof(RDColor) - 1, strlen(color));
        return false;
    }

    if(color[0] != '#') {
        LOG_FAIL("color must start with '#'");
        return false;
    }

    for(unsigned int i = 1; i < sizeof(RDColor) - 1; i++) {
        if(!isxdigit((unsigned char)color[i])) {
            LOG_FAIL("invalid character '%c' at position %d", color[i], i);
            return false;
        }
    }

    return true;
}

void rd_i_theme_init(RDTheme* self) {
    rd_i_theme_set_color(self, RD_THEME_FOREGROUND, "#000000");
    rd_i_theme_set_color(self, RD_THEME_BACKGROUND, "#ffffff");
}

bool rd_i_theme_set_color(RDTheme* self, RDThemeKind kind, const char* color) {
    if(!_rd_theme_validate_color(color)) return false;

    if(kind >= RD_THEME_COUNT) {
        LOG_FAIL("invalid theme kind: %d", kind);
        return false;
    }

    RDColor* field = (RDColor*)((char*)self + RD_THEME_PARAMS[kind].offset);
    memcpy(field, color, sizeof(RDColor));
    return true;
}

bool rd_set_theme_color(RDThemeKind kind, const char* color) {
    return rd_i_theme_set_color(&rd_i_state.theme, kind, color);
}

const char* rd_get_theme_color(RDThemeKind kind) {
    if(kind >= RD_THEME_COUNT) {
        LOG_FAIL("invalid theme kind: %d", kind);
        return NULL;
    }

    const RDThemeParam* param = &RD_THEME_PARAMS[kind];
    const char* color = (const char*)&rd_i_state.theme + param->offset;

    // Resolve alternatives
    while(!(*color) && param->alt) {
        param = &RD_THEME_PARAMS[param->alt];
        color = (const char*)&rd_i_state.theme + param->offset;
    }

    return color;
}
