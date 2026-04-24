#pragma once

#include "support/utils.h"
#include <redasm/graph/graph.h>

typedef struct RDNodeVect {
    RDGraphNode* data;
    usize length;
    usize capacity;
} RDNodeVect;

typedef struct RDEdgeVect {
    RDGraphEdge* data;
    usize length;
    usize capacity;
} RDEdgeVect;

typedef struct RDGraphPointVect {
    RDGraphPoint* data;
    usize length;
    usize capacity;
} RDGraphPointVect;

typedef struct RDNodeAttributes {
    int x, y, width, height;
    RDNodeData data;
} RDNodeAttributes;

typedef struct RDEdgeAttributes {
    RDGraphEdge edge;
    char* label;
    char* color;
    RDGraphPointVect routes;
    RDGraphPointVect arrow;
} RDEdgeAttributes;

typedef struct RDGraph {
    RDCharVect dot_buf;

    RDGraphNode node_id;
    RDGraphNode root;
    RDNodeVect nodes;
    RDEdgeVect edges;
    RDEdgeVect incoming_edges;
    RDEdgeVect outgoing_edges;

    struct {
        RDNodeAttributes* data;
        usize length;
        usize capacity;
    } node_attributes;

    struct {
        RDEdgeAttributes* data;
        usize length;
        usize capacity;
    } edge_attributes;

    int area_width;
    int area_height;
} RDGraph;

static inline usize rd_i_node2index(RDGraphNode n) {
    assert(n && "invalid graph node");
    return n - 1;
}

void rd_i_graph_clear(RDGraph* self);
void rd_i_graph_remove_outgoing_edges(RDGraph* self, RDGraphNode n);
void rd_i_graph_remove_incoming_edges(RDGraph* self, RDGraphNode n);
void rd_i_graph_remove_edges(RDGraph* self, RDGraphNode n);
RDNodeAttributes* rd_i_graph_get_node_attributes(RDGraph* self, RDGraphNode n);
const RDEdgeVect* rd_i_graph_get_outgoing_edges(RDGraph* self, RDGraphNode n);
const RDEdgeVect* rd_i_graph_get_incoming_edges(RDGraph* self, RDGraphNode n);
const RDNodeVect* rd_i_graph_get_nodes(const RDGraph* self);
