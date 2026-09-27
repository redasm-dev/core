#include "graphs/graph.h"
#include "support/containers.h"
#include <assert.h>
#include <math.h>
#include <redasm/allocator.h>
#include <redasm/graph/layout.h>

#define RD_RADIAL_PADDING 24
#define RD_RADIAL_TAU 6.283185307179586 // avoids relying on M_PI's availability

typedef struct RDRadialBlock {
    RDGraphNode node;
    RDNodeVect neighbors;
    RDGraphNode parent;
    RDNodeVect children;
    int ring;
    usize weight;
    double angle;
    int width, height;
    float x, y;
} RDRadialBlock;

typedef struct RDRadialBlockVect {
    RDRadialBlock* data;
    usize length;
    usize capacity;
} RDRadialBlockVect;

typedef struct RDRadialLayout {
    RDGraph* graph;
    RDRadialBlockVect blocks;
    int max_ring;
    int ring_step;
    float cx, cy;
} RDRadialLayout;

// ---------------------------------------------------------------------------
// Helpers: block lookup (nodes are 1-based, array is 0-based)
// ---------------------------------------------------------------------------

static inline RDRadialBlock* _rd_radial_block(RDRadialLayout* ll,
                                              RDGraphNode n) {
    return &ll->blocks.data[rd_i_node2index(n)];
}

// ---------------------------------------------------------------------------
// Step 1: create_blocks
// One RDRadialBlock per real node, plus the undirected adjacency every
// node needs for BFS: every real edge contributes a neighbor entry on
// BOTH ends here, since ring/tree assignment only cares about
// "how many hops", never which way the call points.
// Direction is preserved separately, at drawing time, by re-walking real
// outgoing edges in _rd_radial_route_edges.
// ---------------------------------------------------------------------------

static void _rd_radial_create_blocks(RDRadialLayout* ll) {
    const RDGraphNode* n;
    vect_each(n, &ll->graph->nodes) {
        RDNodeAttributes* attr = rd_i_graph_get_node_attributes(ll->graph, *n);
        assert(attr && "node attributes not found");

        int extent = attr->width > attr->height ? attr->width : attr->height;
        if(extent > ll->ring_step) ll->ring_step = extent;

        vect_push(&ll->blocks, ((RDRadialBlock){
                                   .node = *n,
                                   .width = attr->width,
                                   .height = attr->height,
                               }));
    }

    const RDGraphNode* it;
    vect_each(it, &ll->graph->nodes) {
        const RDEdgeVect* out = rd_i_graph_get_outgoing_edges(ll->graph, *it);
        const RDGraphEdge* e;
        vect_each(e, out) {
            RDRadialBlock* src = _rd_radial_block(ll, *it);
            RDRadialBlock* dst = _rd_radial_block(ll, e->dst);
            vect_push(&src->neighbors, e->dst);
            vect_push(&dst->neighbors, *it);
        }
    }

    // gap between successive rings, scaled to typical node size so labels
    // at adjacent rings don't overlap
    ll->ring_step += RD_RADIAL_PADDING;
}

// ---------------------------------------------------------------------------
// Step 2: build_tree
// Plain undirected BFS from root: assigns ring (hop distance) and a
// spanning-tree parent/children relationship to every node.
// ---------------------------------------------------------------------------

static void _rd_radial_build_tree(RDRadialLayout* ll) {
    usize total = vect_length(&ll->blocks);
    bool* visited = rd_alloc0(total, sizeof(bool));
    assert(visited);

    RDNodeVect queue = {0};
    vect_reserve(&queue, total);

    RDGraphNode root = ll->graph->root;
    visited[rd_i_node2index(root)] = true;
    vect_push(&queue, root);

    usize head = 0;
    while(head < vect_length(&queue)) {
        RDGraphNode cur = queue.data[head++];
        RDRadialBlock* block = _rd_radial_block(ll, cur);

        const RDGraphNode* nb;
        vect_each(nb, &block->neighbors) {
            if(visited[rd_i_node2index(*nb)]) continue;

            visited[rd_i_node2index(*nb)] = true;
            RDRadialBlock* child = _rd_radial_block(ll, *nb);
            child->ring = block->ring + 1;
            child->parent = cur;
            vect_push(&block->children, *nb);
            vect_push(&queue, *nb);

            if(child->ring > ll->max_ring) ll->max_ring = child->ring;
        }
    }

    // defensive only: every node callgraph.c produces is discovered by a
    // walk starting at root, so this shouldn't fire in practice.
    // If this layout is ever handed a graph with a genuinely disconnected node,
    // park it one ring past everything else instead of leaving it at the
    // zero-initialised ring 0, where it would overlap root.
    const RDGraphNode* n;
    vect_each(n, &ll->graph->nodes) {
        if(*n == root || visited[rd_i_node2index(*n)]) continue;

        RDRadialBlock* orphan = _rd_radial_block(ll, *n);
        orphan->ring = ll->max_ring + 1;
        orphan->parent = root;
        vect_push(&_rd_radial_block(ll, root)->children, *n);
        if(orphan->ring > ll->max_ring) ll->max_ring = orphan->ring;
    }

    rd_free(visited);
    vect_destroy(&queue);
}

// ---------------------------------------------------------------------------
// Step 3: compute_weight (post-order recursive)
// A leaf's weight is 1; an internal node's weight is the sum of its
// children's.
// Drives how much of a parent's angular wedge each child gets in the next step.
// ---------------------------------------------------------------------------

static usize _rd_radial_compute_weight(RDRadialLayout* ll,
                                       RDRadialBlock* block) {
    if(vect_is_empty(&block->children)) {
        block->weight = 1;
        return 1;
    }

    usize total = 0;
    const RDGraphNode* n;
    vect_each(n, &block->children) total +=
        _rd_radial_compute_weight(ll, _rd_radial_block(ll, *n));

    block->weight = total;
    return total;
}

// ---------------------------------------------------------------------------
// Step 4: assign_angles (pre-order recursive)
// Subdivides [start, end) among a block's children proportional to each
// child's weight; a block's own angle is the midpoint of the span it was
// given, which is the standard "parent centered over its descendants"
// placement for radial/circular trees.
// ---------------------------------------------------------------------------

static void _rd_radial_assign_angles(RDRadialLayout* ll, RDRadialBlock* block,
                                     double start, double end) {
    block->angle = (start + end) / 2.0;

    if(vect_is_empty(&block->children)) return;

    double span = end - start;
    double cursor = start;

    const RDGraphNode* n;
    vect_each(n, &block->children) {
        RDRadialBlock* child = _rd_radial_block(ll, *n);
        double share = span * ((double)child->weight / (double)block->weight);
        _rd_radial_assign_angles(ll, child, cursor, cursor + share);
        cursor += share;
    }
}

// ---------------------------------------------------------------------------
// Step 5: compute_positions
// x = cx + ring*ring_step*cos(angle), y = cy + ring*ring_step*sin(angle).
// Root (ring 0) always lands exactly at (cx, cy) regardless of its angle.
// ---------------------------------------------------------------------------

static void _rd_radial_compute_positions(RDRadialLayout* ll) {
    for(usize i = 0; i < vect_length(&ll->blocks); i++) {
        RDRadialBlock* block = &ll->blocks.data[i];
        double radius = (double)block->ring * (double)ll->ring_step;

        // block->x/y stay the true CENTER
        block->x = ll->cx + (float)(radius * cos(block->angle));
        block->y = ll->cy + (float)(radius * sin(block->angle));

        rd_graph_set_node_x(ll->graph, block->node,
                            (int)(block->x - ((float)block->width / 2.0F)));
        rd_graph_set_node_y(ll->graph, block->node,
                            (int)(block->y - ((float)block->height / 2.0F)));
    }
}

// ---------------------------------------------------------------------------
// Step 6: route_edges
// Walks REAL outgoing edges (not the spanning tree) so every actual call
// gets drawn, in its real direction ring/angle only ever decided where
// a node sits, never which edges exist.
// ---------------------------------------------------------------------------

// Distance from a rect's center, along a given direction, to where that
// ray exits the rect.
// i.e. where a line from the center should stop to land exactly on the box's
// edge, not some circular approximation of it.
static float _rd_radial_clip_to_box(float ux, float uy, float hw, float hh) {
    float tx = fabsf(ux) > 0.0001F ? hw / fabsf(ux) : 1e9F;
    float ty = fabsf(uy) > 0.0001F ? hh / fabsf(uy) : 1e9F;
    return tx < ty ? tx : ty;
}

static void _rd_radial_route_edges(RDRadialLayout* ll) {
    const RDGraphNode* n;
    vect_each(n, &ll->graph->nodes) {
        RDRadialBlock* src = _rd_radial_block(ll, *n);
        const RDEdgeVect* out = rd_i_graph_get_outgoing_edges(ll->graph, *n);

        const RDGraphEdge* e;
        vect_each(e, out) {
            RDRadialBlock* dst = _rd_radial_block(ll, e->dst);

            float dx = dst->x - src->x;
            float dy = dst->y - src->y;
            float len = sqrtf((dx * dx) + (dy * dy));
            if(len < 1.0F) continue; // overlapping centers, nothing to draw

            float ux = dx / len, uy = dy / len;

            // exact rect-edge clipping, not a bounding-circle guess: a
            // circle sized off the longer side badly over-trims a wide,
            // short label box whenever the line isn't near-horizontal
            float src_t = _rd_radial_clip_to_box(
                ux, uy, (float)src->width / 2.0F, (float)src->height / 2.0F);
            float dst_t = _rd_radial_clip_to_box(
                ux, uy, (float)dst->width / 2.0F, (float)dst->height / 2.0F);

            RDGraphPoint start = {.x = (int)(src->x + (ux * src_t)),
                                  .y = (int)(src->y + (uy * src_t))};
            RDGraphPoint tip = {.x = (int)(dst->x - (ux * dst_t)),
                                .y = (int)(dst->y - (uy * dst_t))};

            RDGraphPointVect routes = {0};
            vect_push(&routes, start);
            vect_push(&routes, tip);

            // Arrowhead size scales down on short visible segments so it
            // can never extend past 'start'.
            // A fixed 6px back offset is safe for them because
            // their edges always span at least a full grid row, but a
            // radial edge between two close nodes on adjacent rings can
            // end up shorter than that after exact rect clipping, which
            // would otherwise push the arrowhead behind/inside the
            // source node where it's hidden rather than just small.
            float visible_len = len - src_t - dst_t;
            float arrow_back =
                visible_len < 12.0F ? (visible_len / 2.0F) : 6.0F;
            if(arrow_back < 1.0F) arrow_back = 1.0F;
            float arrow_half = arrow_back / 2.0F;

            // arrowhead: 3-point triangle, rotated to the line's own
            // direction rather than always pointing "down".
            float bx = -ux, by = -uy; // back along the line
            float px = -uy, py = ux;  // perpendicular

            RDGraphPointVect arrow = {0};
            vect_push(&arrow, ((RDGraphPoint){
                                  .x = (int)(tip.x + (int)(bx * arrow_back) +
                                             (int)(px * arrow_half)),
                                  .y = (int)(tip.y + (int)(by * arrow_back) +
                                             (int)(py * arrow_half))}));
            vect_push(&arrow, ((RDGraphPoint){
                                  .x = (int)(tip.x + (int)(bx * arrow_back) -
                                             (int)(px * arrow_half)),
                                  .y = (int)(tip.y + (int)(by * arrow_back) -
                                             (int)(py * arrow_half))}));
            vect_push(&arrow, tip);

            rd_graph_set_edge_routes(ll->graph, e, routes.data,
                                     vect_length(&routes));
            rd_graph_set_edge_arrow(ll->graph, e, arrow.data,
                                    vect_length(&arrow));

            vect_destroy(&routes);
            vect_destroy(&arrow);
        }
    }
}

// ---------------------------------------------------------------------------
// Step 7: compute_area
// Bounding box of every node's actual drawn extent, for canvas sizing.
// ---------------------------------------------------------------------------

static void _rd_radial_compute_area(RDRadialLayout* ll) {
    float minx = ll->cx, maxx = ll->cx, miny = ll->cy, maxy = ll->cy;

    for(usize i = 0; i < vect_length(&ll->blocks); i++) {
        RDRadialBlock* b = &ll->blocks.data[i];
        float hw = (float)b->width / 2.0F, hh = (float)b->height / 2.0F;

        if((b->x - hw) < minx) minx = b->x - hw;
        if((b->x + hw) > maxx) maxx = b->x + hw;
        if((b->y - hh) < miny) miny = b->y - hh;
        if((b->y + hh) > maxy) maxy = b->y + hh;
    }

    rd_graph_set_area_width(ll->graph,
                            (int)(maxx - minx) + (RD_RADIAL_PADDING * 2));
    rd_graph_set_area_height(ll->graph,
                             (int)(maxy - miny) + (RD_RADIAL_PADDING * 2));
}

// ---------------------------------------------------------------------------
// Cleanup
// ---------------------------------------------------------------------------

static void _rd_radial_destroy(RDRadialLayout* ll) {
    for(usize i = 0; i < vect_length(&ll->blocks); i++) {
        RDRadialBlock* b = &ll->blocks.data[i];
        vect_destroy(&b->neighbors);
        vect_destroy(&b->children);
    }
    vect_destroy(&ll->blocks);
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

bool rd_graph_compute_radial(RDGraph* self) {
    if(!self || !self->root) return false;

    RDRadialLayout ll = {.graph = self};

    usize node_count = vect_length(&self->nodes);
    vect_reserve(&ll.blocks, node_count); // stable pointers throughout

    _rd_radial_create_blocks(&ll);
    _rd_radial_build_tree(&ll);
    _rd_radial_compute_weight(&ll, _rd_radial_block(&ll, self->root));
    _rd_radial_assign_angles(&ll, _rd_radial_block(&ll, self->root), 0.0,
                             RD_RADIAL_TAU);

    // center offset large enough that no node's radius*ring_step reach
    // can go negative, given ring_step already accounts for max node size
    ll.cx = ll.cy =
        (float)((ll.max_ring + 1) * ll.ring_step) + RD_RADIAL_PADDING;

    _rd_radial_compute_positions(&ll);
    _rd_radial_route_edges(&ll);
    _rd_radial_compute_area(&ll);

    _rd_radial_destroy(&ll);
    return true;
}
