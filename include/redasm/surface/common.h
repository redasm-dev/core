#pragma once

#include <redasm/config.h>
#include <redasm/theme.h>

typedef struct RDCell {
    char ch;
    RDThemeKind fg;
    RDThemeKind bg;
} RDCell;

typedef struct RDRowSlice {
    const RDCell* data;
    usize length;
    usize content_length;
} RDRowSlice;

typedef struct RDSurfacePos {
    int row;
    int col;
} RDSurfacePos;
