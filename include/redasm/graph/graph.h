#pragma once

#include <redasm/common.h>
#include <redasm/config.h>

typedef struct RDGraph RDGraph;
typedef usize RDGraphNode;
typedef uptr RDNodeData;

typedef struct RDGraphEdge {
    RDGraphNode src, dst;
} RDGraphEdge;

typedef struct RDGraphPoint {
    int x, y;
} RDGraphPoint;

typedef struct RDNodeSlice {
    const RDGraphNode* data;
    usize length;
} RDNodeSlice;

typedef struct RDEdgeSlice {
    const RDGraphEdge* data;
    usize length;
} RDEdgeSlice;

typedef struct RDGraphPointSlice {
    const RDGraphPoint* data;
    usize length;
} RDGraphPointSlice;

RD_API RDGraph* rd_graph_create(void);
RD_API void rd_graph_destroy(RDGraph* self);
RD_API u32 rd_graph_get_hash(const RDGraph* self);
RD_API const char* rd_graph_generate_dot(RDGraph* self);
RD_API RDGraphNode rd_graph_add_node(RDGraph* self);
RD_API RDGraphEdge rd_graph_add_edge(RDGraph* self, RDGraphNode src,
                                     RDGraphNode dst);
RD_API void rd_graph_remove_node(RDGraph* self, RDGraphNode n);
RD_API void rd_graph_remove_edge(RDGraph* self, const RDGraphEdge* e);
RD_API bool rd_graph_is_empty(const RDGraph* self);
RD_API bool rd_graph_set_root(RDGraph* self, RDGraphNode n);
RD_API RDGraphNode rd_graph_get_root(const RDGraph* self);
RD_API RDNodeSlice rd_graph_get_nodes(const RDGraph* self);
RD_API RDEdgeSlice rd_graph_get_edges(const RDGraph* self);
RD_API RDEdgeSlice rd_graph_get_outgoing_edges(RDGraph* self, RDGraphNode n);
RD_API RDEdgeSlice rd_graph_get_incoming_edges(RDGraph* self, RDGraphNode n);
RD_API const RDGraphEdge* rd_graph_find_edge(const RDGraph* self,
                                             RDGraphNode src, RDGraphNode dst);

RD_API bool rd_graph_set_data(RDGraph* self, RDGraphNode n, RDNodeData d);
RD_API RDNodeData rd_graph_get_data(const RDGraph* self, RDGraphNode n);

RD_API void rd_graph_clear_layout(RDGraph* self);

RD_API int rd_graph_get_area_width(const RDGraph* self);
RD_API int rd_graph_get_area_height(const RDGraph* self);
RD_API void rd_graph_set_area_width(RDGraph* self, int w);
RD_API void rd_graph_set_area_height(RDGraph* self, int h);

RD_API int rd_graph_get_node_x(const RDGraph* self, RDGraphNode n);
RD_API int rd_graph_get_node_y(const RDGraph* self, RDGraphNode n);
RD_API int rd_graph_get_node_width(const RDGraph* self, RDGraphNode n);
RD_API int rd_graph_get_node_height(const RDGraph* self, RDGraphNode n);

RD_API void rd_graph_set_node_x(RDGraph* self, RDGraphNode n, int x);
RD_API void rd_graph_set_node_y(RDGraph* self, RDGraphNode n, int y);
RD_API void rd_graph_set_node_width(RDGraph* self, RDGraphNode n, int w);
RD_API void rd_graph_set_node_height(RDGraph* self, RDGraphNode n, int h);

RD_API const char* rd_graph_get_edge_color(const RDGraph* self,
                                           const RDGraphEdge* e);

RD_API const char* rd_graph_get_edge_label(const RDGraph* self,
                                           const RDGraphEdge* e);

RD_API RDGraphPointSlice rd_graph_get_edge_routes(const RDGraph* self,
                                                  const RDGraphEdge* e);

RD_API RDGraphPointSlice rd_graph_get_edge_arrow(const RDGraph* self,
                                                 const RDGraphEdge* e);

RD_API void rd_graph_set_edge_color(RDGraph* self, const RDGraphEdge* e,
                                    const char* s);

RD_API void rd_graph_set_edge_label(RDGraph* self, const RDGraphEdge* e,
                                    const char* s);

RD_API void rd_graph_set_edge_routes(RDGraph* self, const RDGraphEdge* e,
                                     const RDGraphPoint* poly, usize n);

RD_API void rd_graph_set_edge_arrow(RDGraph* self, const RDGraphEdge* e,
                                    const RDGraphPoint* poly, usize n);
