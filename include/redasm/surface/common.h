#pragma once

#include <redasm/config.h>
#include <redasm/theme.h>

typedef enum {
    RD_RF_DEFAULT = 0U,
    RD_RF_NO_INDENT = 1U << 0,
    RD_RF_NO_ADDRESS = 1U << 1,
    RD_RF_NO_SEGMENT = 1U << 2,
    RD_RF_NO_FUNCTION = 1U << 3,
    RD_RF_NO_REFS = 1U << 4,
    RD_RF_NO_COMMENTS = 1U << 5,
    RD_RF_NO_CURSOR = 1U << 6,
    RD_RF_NO_CURSOR_LINE = 1U << 7,
    RD_RF_NO_SELECTION = 1U << 8,
    RD_RF_NO_HIGHLIGHT = 1U << 9,
    RD_RF_NO_NAMES = 1U << 10,
} RDRenderFlags;

#define RD_RF_GRAPH                                                            \
    ((RDRenderFlags)(RD_RF_NO_ADDRESS | RD_RF_NO_INDENT | RD_RF_NO_SEGMENT |   \
                     RD_RF_NO_FUNCTION | RD_RF_NO_COMMENTS))

#define RD_RF_POPUP                                                            \
    ((RDRenderFlags)(RD_RF_NO_ADDRESS | RD_RF_NO_CURSOR | RD_RF_NO_SELECTION | \
                     RD_RF_NO_CURSOR_LINE))

#define RD_RF_TEXT ((RDRenderFlags)~0U)

typedef enum {
    RD_RM_NORMAL = 0,
    RD_RM_RDIL,
    RD_RM_FLAGS,
} RDRenderMode;

typedef enum {
    RD_NUM_DEFAULT = 0,
    RD_NUM_SIGNED = 1U << 0,
    RD_NUM_PREFIX = 1U << 1,
    RD_NUM_NOADDR = 1U << 1,
} RDNumberFlags;

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
