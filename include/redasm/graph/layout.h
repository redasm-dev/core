#pragma once

#include <redasm/config.h>

typedef struct RDGraph RDGraph;

typedef enum {
    RD_LAYERED_LAYOUT_MEDIUM = 0,
    RD_LAYERED_LAYOUT_NARROW,
    RD_LAYERED_LAYOUT_WIDE,
} RDLayeredLayoutKind;

typedef enum {
    RD_EGO_LAYOUT_MEDIUM = 0,
    RD_EGO_LAYOUT_NARROW,
    RD_EGO_LAYOUT_WIDE,
} RDEgoLayoutKind;

RD_API bool rd_graph_compute_layered(RDGraph* self, RDLayeredLayoutKind kind);
RD_API bool rd_graph_compute_ego(RDGraph* self, RDEgoLayoutKind kind);
RD_API bool rd_graph_compute_radial(RDGraph* self);
