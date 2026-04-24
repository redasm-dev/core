#include "graph.h"
#include "support/containers.h"
#include "support/hash.h"
#include "support/logging.h"
#include <redasm/support/utils.h>
#include <redasm/theme.h>
#include <stdlib.h>

static void _rd_graph_destroy_edge_attribute(RDEdgeAttributes* ea) {
    vect_destroy(&ea->arrow);
    vect_destroy(&ea->routes);
    rd_free(ea->label);
    rd_free(ea->color);
    ea->label = NULL;
    ea->color = NULL;
}

static void _rd_graph_destroy_edge_attributes(RDGraph* self) {
    RDEdgeAttributes* it;
    vect_each(it, &self->edge_attributes) {
        _rd_graph_destroy_edge_attribute(it);
    }
}

static RDEdgeAttributes* _rd_graph_find_edge_attributes(const RDGraph* self,
                                                        const RDGraphEdge* e) {
    if(!e || !e->src || !e->dst) return NULL;

    RDEdgeAttributes* it;
    vect_each(it, &self->edge_attributes) {
        if(it->edge.src == e->src && it->edge.dst == e->dst) return it;
    }

    return NULL;
}

void rd_i_graph_clear(RDGraph* self) {
    _rd_graph_destroy_edge_attributes(self);
    vect_clear(&self->edge_attributes);
    vect_clear(&self->node_attributes);
    vect_clear(&self->incoming_edges);
    vect_clear(&self->outgoing_edges);
    vect_clear(&self->edges);
    vect_clear(&self->nodes);
    self->node_id = 0;
    self->root = 0;
}

RDNodeAttributes* rd_i_graph_get_node_attributes(RDGraph* self, RDGraphNode n) {
    if(!n) return false;

    if(rd_i_node2index(n) < vect_length(&self->node_attributes))
        return vect_at(&self->node_attributes, rd_i_node2index(n));

    LOG_WARN("node out of range");
    return false;
}

const RDEdgeVect* rd_i_graph_get_outgoing_edges(RDGraph* self, RDGraphNode n) {
    vect_clear(&self->outgoing_edges);

    const RDGraphEdge* e;
    vect_each(e, &self->edges) {
        if(e->src == n) vect_push(&self->outgoing_edges, *e);
    }

    return &self->outgoing_edges;
}

const RDEdgeVect* rd_i_graph_get_incoming_edges(RDGraph* self, RDGraphNode n) {
    vect_clear(&self->incoming_edges);

    const RDGraphEdge* e;
    vect_each(e, &self->edges) {
        if(e->dst == n) vect_push(&self->incoming_edges, *e);
    }

    return &self->incoming_edges;
}

const RDNodeVect* rd_i_graph_get_nodes(const RDGraph* self) {
    return &self->nodes;
}

void rd_i_graph_remove_outgoing_edges(RDGraph* self, RDGraphNode n) {
    const RDEdgeVect* out_edges = rd_i_graph_get_outgoing_edges(self, n);

    const RDGraphEdge* e;
    vect_each(e, out_edges) { rd_graph_remove_edge(self, e); }
}

void rd_i_graph_remove_incoming_edges(RDGraph* self, RDGraphNode n) {
    const RDEdgeVect* inc_edges = rd_i_graph_get_incoming_edges(self, n);

    const RDGraphEdge* e;
    vect_each(e, inc_edges) { rd_graph_remove_edge(self, e); }
}

void rd_i_graph_remove_edges(RDGraph* self, RDGraphNode n) {
    for(usize i = 0; i < vect_length(&self->edges);) {
        RDGraphEdge* e = vect_at(&self->edges, i);

        if(e->src == n || e->dst == n)
            vect_remove(&self->edges, e, 1);
        else
            i++;
    }
}

RDGraph* rd_graph_create(void) { return calloc(1, sizeof(RDGraph)); }

void rd_graph_destroy(RDGraph* self) {
    if(!self) return;

    _rd_graph_destroy_edge_attributes(self);
    vect_destroy(&self->edge_attributes);
    vect_destroy(&self->node_attributes);
    vect_destroy(&self->incoming_edges);
    vect_destroy(&self->outgoing_edges);
    vect_destroy(&self->edges);
    vect_destroy(&self->nodes);
    vect_destroy(&self->dot_buf);
    self->node_id = 0;
    self->root = 0;
    free(self);
}

void rd_graph_clear_layout(RDGraph* self) {
    RDNodeAttributes* na; // preserve data
    vect_each(na, &self->node_attributes) {
        *na = (RDNodeAttributes){.data = na->data};
    }

    RDEdgeAttributes* ea; // preserve colors and labels
    vect_each(ea, &self->edge_attributes) {
        vect_clear(&ea->routes);
        vect_clear(&ea->arrow);
    }

    self->area_width = 0;
    self->area_height = 0;
}

RDGraphNode rd_graph_add_node(RDGraph* self) {
    RDGraphNode n = ++self->node_id;
    vect_push(&self->nodes, n);
    vect_push(&self->node_attributes, (RDNodeAttributes){0});
    return n;
}

RDGraphEdge rd_graph_add_edge(RDGraph* self, RDGraphNode src, RDGraphNode dst) {
    RDGraphEdge e = {.src = src, .dst = dst};

    if(!rd_graph_find_edge(self, src, dst)) {
        vect_push(&self->edges, e);
        vect_push(&self->edge_attributes, (RDEdgeAttributes){.edge = e});
    }

    return e;
}

void rd_graph_remove_node(RDGraph* self, RDGraphNode n) {
    if(!n) return;

    RDGraphNode* it;
    vect_each(it, &self->nodes) {
        if(*it == n) {
            vect_remove(&self->nodes, it, 1);
            vect_remove_at(&self->node_attributes, rd_i_node2index(n), 1);
            break;
        }
    }
}

void rd_graph_remove_edge(RDGraph* self, const RDGraphEdge* e) {
    if(!e) return;

    RDGraphEdge* it;
    vect_each(it, &self->edges) {
        if(it->src == e->src && it->dst == e->dst) {
            vect_remove(&self->edges, it, 1);

            RDEdgeAttributes* ea = _rd_graph_find_edge_attributes(self, e);
            assert(ea && "invalid edge attributes");
            _rd_graph_destroy_edge_attribute(ea);
            vect_remove(&self->edge_attributes, ea, 1);
            break;
        }
    }
}

bool rd_graph_is_empty(const RDGraph* self) {
    return vect_is_empty(&self->nodes);
}

bool rd_graph_set_root(RDGraph* self, RDGraphNode n) {
    const RDGraphNode* it;

    vect_each(it, &self->nodes) {
        if(*it == n) {
            self->root = n;
            return true;
        }
    }

    return false;
}

bool rd_graph_set_data(RDGraph* self, RDGraphNode n, RDNodeData d) {
    if(!n) return false;

    if(rd_i_node2index(n) < vect_length(&self->node_attributes)) {
        vect_at(&self->node_attributes, rd_i_node2index(n))->data = d;
        return true;
    }

    LOG_WARN("node out of range");
    return false;
}

RDNodeData rd_graph_get_data(const RDGraph* self, RDGraphNode n) {
    if(!n) return 0;

    if(rd_i_node2index(n) < vect_length(&self->node_attributes))
        return vect_at(&self->node_attributes, rd_i_node2index(n))->data;

    return 0;
}

RDGraphNode rd_graph_get_root(const RDGraph* self) { return self->root; }

RDNodeSlice rd_graph_get_nodes(const RDGraph* self) {
    return vect_to_slice(RDNodeSlice, &self->nodes);
}

RDEdgeSlice rd_graph_get_edges(const RDGraph* self) {
    return vect_to_slice(RDEdgeSlice, &self->edges);
}

RDEdgeSlice rd_graph_get_outgoing_edges(RDGraph* self, RDGraphNode n) {
    return vect_to_slice(RDEdgeSlice, rd_i_graph_get_outgoing_edges(self, n));
}

RDEdgeSlice rd_graph_get_incoming_edges(RDGraph* self, RDGraphNode n) {
    return vect_to_slice(RDEdgeSlice, rd_i_graph_get_incoming_edges(self, n));
}

const RDGraphEdge* rd_graph_find_edge(const RDGraph* self, RDGraphNode src,
                                      RDGraphNode dst) {
    const RDGraphEdge* e;
    vect_each(e, &self->edges) {
        if(e->src == src && e->dst == dst) return e;
    }

    return NULL;
}

int rd_graph_get_area_width(const RDGraph* self) { return self->area_width; }
int rd_graph_get_area_height(const RDGraph* self) { return self->area_height; }
void rd_graph_set_area_width(RDGraph* self, int w) { self->area_width = w; }
void rd_graph_set_area_height(RDGraph* self, int h) { self->area_height = h; }

int rd_graph_get_node_x(const RDGraph* self, RDGraphNode n) {
    if(!n) return -1;

    if(rd_i_node2index(n) < vect_length(&self->node_attributes))
        return vect_at(&self->node_attributes, rd_i_node2index(n))->x;

    LOG_WARN("cannot get x-coordinate, node out of range");
    return -1;
}

int rd_graph_get_node_y(const RDGraph* self, RDGraphNode n) {
    if(!n) return -1;

    if(rd_i_node2index(n) < vect_length(&self->node_attributes))
        return vect_at(&self->node_attributes, rd_i_node2index(n))->y;

    LOG_WARN("cannot get y-coordinate, node out of range");
    return -1;
}

int rd_graph_get_node_width(const RDGraph* self, RDGraphNode n) {
    if(!n) return -1;

    if(rd_i_node2index(n) < vect_length(&self->node_attributes))
        return vect_at(&self->node_attributes, rd_i_node2index(n))->width;

    LOG_WARN("cannot get width, node out of range");
    return -1;
}

int rd_graph_get_node_height(const RDGraph* self, RDGraphNode n) {
    if(!n) return -1;

    if(rd_i_node2index(n) < vect_length(&self->node_attributes))
        return vect_at(&self->node_attributes, rd_i_node2index(n))->height;

    LOG_WARN("cannot get height, node out of range");
    return -1;
}

void rd_graph_set_node_x(RDGraph* self, RDGraphNode n, int x) {
    if(!n) return;

    if(rd_i_node2index(n) < vect_length(&self->node_attributes))
        vect_at(&self->node_attributes, rd_i_node2index(n))->x = x;
    else
        LOG_WARN("cannot set x-coordinate, node out of range");
}

void rd_graph_set_node_y(RDGraph* self, RDGraphNode n, int y) {
    if(!n) return;

    if(rd_i_node2index(n) < vect_length(&self->node_attributes))
        vect_at(&self->node_attributes, rd_i_node2index(n))->y = y;
    else
        LOG_WARN("cannot set y-coordinate, node out of range");
}

void rd_graph_set_node_width(RDGraph* self, RDGraphNode n, int w) {
    if(!n) return;

    if(rd_i_node2index(n) < vect_length(&self->node_attributes))
        vect_at(&self->node_attributes, rd_i_node2index(n))->width = w;
    else
        LOG_WARN("cannot set width, node out of range");
}

void rd_graph_set_node_height(RDGraph* self, RDGraphNode n, int h) {
    if(!n) return;

    if(rd_i_node2index(n) < vect_length(&self->node_attributes))
        vect_at(&self->node_attributes, rd_i_node2index(n))->height = h;
    else
        LOG_WARN("cannot set height, node out of range");
}

const char* rd_graph_get_edge_color(const RDGraph* self, const RDGraphEdge* e) {
    const RDEdgeAttributes* ea = _rd_graph_find_edge_attributes(self, e);
    if(ea && ea->color) return ea->color;

    return rd_get_theme_color(RD_THEME_FOREGROUND);
}

const char* rd_graph_get_edge_label(const RDGraph* self, const RDGraphEdge* e) {
    const RDEdgeAttributes* ea = _rd_graph_find_edge_attributes(self, e);
    if(ea && ea->label) return ea->label;
    return "";
}

RDGraphPointSlice rd_graph_get_edge_routes(const RDGraph* self,
                                           const RDGraphEdge* e) {
    const RDEdgeAttributes* ea = _rd_graph_find_edge_attributes(self, e);
    if(ea) return vect_to_slice(RDGraphPointSlice, &ea->routes);
    return (RDGraphPointSlice){0};
}

RDGraphPointSlice rd_graph_get_edge_arrow(const RDGraph* self,
                                          const RDGraphEdge* e) {
    const RDEdgeAttributes* ea = _rd_graph_find_edge_attributes(self, e);
    if(ea) return vect_to_slice(RDGraphPointSlice, &ea->arrow);
    return (RDGraphPointSlice){0};
}

void rd_graph_set_edge_color(RDGraph* self, const RDGraphEdge* e,
                             const char* s) {
    RDEdgeAttributes* ea = _rd_graph_find_edge_attributes(self, e);
    if(!ea) return;

    rd_free(ea->color);
    ea->color = rd_strdup(s);
}

void rd_graph_set_edge_label(RDGraph* self, const RDGraphEdge* e,
                             const char* s) {
    RDEdgeAttributes* ea = _rd_graph_find_edge_attributes(self, e);
    if(!ea) return;

    rd_free(ea->label);
    ea->label = rd_strdup(s);
}

void rd_graph_set_edge_routes(RDGraph* self, const RDGraphEdge* e,
                              const RDGraphPoint* poly, usize n) {
    RDEdgeAttributes* ea = _rd_graph_find_edge_attributes(self, e);
    if(!ea) return;

    vect_clear(&ea->routes);
    vect_reserve(&ea->routes, n);

    for(usize i = 0; i < n; i++)
        vect_push(&ea->routes, poly[i]);
}

void rd_graph_set_edge_arrow(RDGraph* self, const RDGraphEdge* e,
                             const RDGraphPoint* poly, usize n) {
    RDEdgeAttributes* ea = _rd_graph_find_edge_attributes(self, e);
    if(!ea) return;

    vect_clear(&ea->arrow);
    vect_reserve(&ea->arrow, n);

    for(usize i = 0; i < n; i++)
        vect_push(&ea->arrow, poly[i]);
}

u32 rd_graph_get_hash(const RDGraph* self) {
    rd_graph_generate_dot((RDGraph*)self);
    return rd_i_murmur3(self->dot_buf.data, self->dot_buf.length);
}

const char* rd_graph_generate_dot(RDGraph* self) {
    str_clear(&self->dot_buf);
    str_append(&self->dot_buf, "digraph G{\n");

    const RDGraphNode* n;
    vect_each(n, &self->nodes) {
        const RDEdgeVect* edges = rd_i_graph_get_outgoing_edges(self, *n);

        const RDGraphEdge* e;
        vect_each(e, edges) {
            str_append(&self->dot_buf, "\t\"#");
            str_append(&self->dot_buf, rd_i_to_dec(e->src));

            str_append(&self->dot_buf, " -> \"#");
            str_append(&self->dot_buf, rd_i_to_dec(e->dst));

            str_append(&self->dot_buf, "\";\n");
        }
    }

    str_push(&self->dot_buf, '}');
    return self->dot_buf.data;
}
