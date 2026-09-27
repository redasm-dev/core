#include "graphs/graph.h"
#include "support/containers.h"
#include <assert.h>
#include <redasm/allocator.h>
#include <redasm/graph/layout.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define RD_EGO_PADDING 16
#define RD_EGO_PADDING_DIV2 ((int)(RD_EGO_PADDING / 2.0F))
#define RD_EGO_PADDING_DIV4 ((int)(RD_EGO_PADDING_DIV2 / 2.0F))
#define RD_EGO_NODE_PADDING (2 * RD_EGO_PADDING)

// ---------------------------------------------------------------------------
// Forward declaration (RDEgoBlock and RDEgoEdge reference each other)
// ---------------------------------------------------------------------------

typedef struct RDEgoBlock RDEgoBlock;

// ---------------------------------------------------------------------------
// Point: a waypoint on a routed edge (grid coordinates, not pixels yet)
// ---------------------------------------------------------------------------

typedef struct RDEgoPoint {
    int row, col, index;
} RDEgoPoint;

typedef struct RDEgoPointVect {
    RDEgoPoint* data;
    usize length;
    usize capacity;
} RDEgoPointVect;

// ---------------------------------------------------------------------------
// Edge: owns its waypoints and final pixel routes
// src_block / dst_block are stable pointers (blocks vect is pre-reserved)
// ---------------------------------------------------------------------------

typedef struct RDEgoEdge {
    RDEgoBlock* src_block;
    RDEgoBlock* dst_block;
    RDGraphPointVect routes;
    RDGraphPointVect arrow;
    int start_index;
    RDEgoPointVect points;
} RDEgoEdge;

typedef struct RDEgoEdgeVect {
    RDEgoEdge* data;
    usize length;
    usize capacity;
} RDEgoEdgeVect;

// ---------------------------------------------------------------------------
// Block: one per REAL graph node.
// The synthetic super-root (see below) is a separate, single instance living
// outside this vector. It never corresponds to a real RDGraphNode, so it can't
// be indexed the way real blocks are.
// ---------------------------------------------------------------------------

struct RDEgoBlock {
    RDGraphNode node;
    RDNodeVect incoming;
    RDNodeVect new_outgoing;
    RDEgoEdgeVect routed_edges;
    float x, y;
    int width, height;
    int col, colcount;
    int row, rowcount;
};

typedef struct RDEgoBlockVect {
    RDEgoBlock* data;
    usize length;
    usize capacity;
} RDEgoBlockVect;

// ---------------------------------------------------------------------------
// Layout state: everything lives here, freed at the end
// ---------------------------------------------------------------------------

typedef struct RDEgoLayout {
    RDGraph* graph;
    RDEgoLayoutKind kind;

    RDEgoBlock vroot;

    RDEgoBlockVect blocks;
    RDNodeVect block_order;

    int rowcount;
    int colcount;

    int* horiz_lanes;
    int* vert_lanes;
    bool* edge_valid;

    int* col_width;
    int* row_height;
    int* col_x;
    int* row_y;
    int* col_edge_x;
    int* row_edge_y;
    int* col_edge_count;
    int* row_edge_count;
} RDEgoLayout;

// ---------------------------------------------------------------------------
// Helpers: block lookup (nodes are 1-based, array is 0-based).
// Only ever called with real nodes, the vroot is accessed directly via
// &ll->vroot, never through this indexed lookup.
// ---------------------------------------------------------------------------

static inline RDEgoBlock* _rd_ego_block(RDEgoLayout* ll, RDGraphNode n) {
    return &ll->blocks.data[rd_i_node2index(n)];
}

// ---------------------------------------------------------------------------
// Grid helpers
// ---------------------------------------------------------------------------

static inline int _rd_ego_stride(const RDEgoLayout* ll) {
    return ll->colcount + 1;
}

static inline int _rd_ego_horiz(const RDEgoLayout* ll, int row, int col) {
    return ll->horiz_lanes[(row * _rd_ego_stride(ll)) + col];
}

static inline void _rd_ego_horiz_set(RDEgoLayout* ll, int row, int col, int v) {
    ll->horiz_lanes[(row * _rd_ego_stride(ll)) + col] = v;
}

static inline int _rd_ego_vert(const RDEgoLayout* ll, int row, int col) {
    return ll->vert_lanes[(row * _rd_ego_stride(ll)) + col];
}

static inline void _rd_ego_vert_set(RDEgoLayout* ll, int row, int col, int v) {
    ll->vert_lanes[(row * _rd_ego_stride(ll)) + col] = v;
}

static inline bool _rd_ego_valid(const RDEgoLayout* ll, int row, int col) {
    return ll->edge_valid[(row * _rd_ego_stride(ll)) + col];
}

static inline void _rd_ego_valid_set(RDEgoLayout* ll, int row, int col,
                                     bool v) {
    ll->edge_valid[(row * _rd_ego_stride(ll)) + col] = v;
}

// ---------------------------------------------------------------------------
// Edge point helpers
// ---------------------------------------------------------------------------

static void _rd_ego_edge_add_point(RDEgoEdge* e, int row, int col, int index) {
    usize len = vect_length(&e->points);
    if(len > 0) vect_at(&e->points, len - 1)->index = index;
    vect_push(&e->points, ((RDEgoPoint){.row = row, .col = col, .index = 0}));
}

// ---------------------------------------------------------------------------
// Step 1: create_blocks
// Build one EgoBlock per real node and populate incoming lists.
// ---------------------------------------------------------------------------

static void _rd_ego_create_blocks(RDEgoLayout* ll) {
    const RDGraphNode* n;
    vect_each(n, &ll->graph->nodes) {
        RDNodeAttributes* attr = rd_i_graph_get_node_attributes(ll->graph, *n);
        assert(attr && "node attributes not found");
        attr->height += RD_EGO_NODE_PADDING;

        // vect_reserve guarantees no realloc, so push is safe
        vect_push(&ll->blocks, ((RDEgoBlock){
                                   .node = *n,
                                   .width = attr->width,
                                   .height = attr->height,
                               }));
    }

    // populate incoming lists by walking all outgoing edges
    const RDEgoBlock* b;
    vect_each(b, &ll->blocks) {
        const RDEdgeVect* out =
            rd_i_graph_get_outgoing_edges(ll->graph, b->node);
        const RDGraphEdge* e;
        vect_each(e, out) {
            RDEgoBlock* dst = _rd_ego_block(ll, e->dst);
            vect_push(&dst->incoming, b->node);
        }
    }
}

// ---------------------------------------------------------------------------
// Step 2: make_acyclic
// BFS spanning tree: each real node becomes a child of exactly one parent.
// Result is stored in block->new_outgoing.
// ---------------------------------------------------------------------------

static void _rd_ego_make_acyclic(RDEgoLayout* ll) {
    usize total = vect_length(&ll->blocks);
    bool* visited = rd_alloc0(total, sizeof(bool));
    assert(visited);

    RDNodeVect queue = {0};
    vect_reserve(&queue, total);

    const RDEgoBlock* seed;
    vect_each(seed, &ll->blocks) {
        if(!vect_is_empty(&seed->incoming)) continue;

        visited[rd_i_node2index(seed->node)] = true;
        vect_push(&queue, seed->node);
        vect_push(&ll->vroot.new_outgoing, seed->node);
    }

    if(vect_is_empty(&queue)) {
        // fully cyclic, no external entry anywhere: fall back to the
        // queried function alone
        RDGraphNode root = ll->graph->root;
        visited[rd_i_node2index(root)] = true;
        vect_push(&queue, root);
        vect_push(&ll->vroot.new_outgoing, root);
    }

    bool changed = true;

    while(changed) {
        changed = false;

        // process queue: nodes with a single remaining incoming edge
        usize head = 0;
        while(head < vect_length(&queue)) {
            RDGraphNode cur = queue.data[head++];
            RDEgoBlock* block = _rd_ego_block(ll, cur);
            vect_push(&ll->block_order, cur);

            const RDEdgeVect* out =
                rd_i_graph_get_outgoing_edges(ll->graph, cur);
            const RDGraphEdge* e;
            vect_each(e, out) {
                if(visited[rd_i_node2index(e->dst)]) continue;

                RDEgoBlock* dst = _rd_ego_block(ll, e->dst);

                if(vect_length(&dst->incoming) == 1) {
                    // only one unvisited parent left: add as tree child
                    vect_clear(&dst->incoming);
                    vect_push(&block->new_outgoing, e->dst);
                    vect_push(&queue, e->dst);
                    visited[rd_i_node2index(e->dst)] = true;
                    changed = true;
                }
                else {
                    // remove this parent from dst's incoming list
                    RDGraphNode* it;
                    vect_each(it, &dst->incoming) {
                        if(*it == cur) {
                            vect_del(&dst->incoming,
                                     vect_idx(&dst->incoming, it), 1);
                            break;
                        }
                    }
                }
            }
        }
        // reset queue for next pass
        vect_clear(&queue);

        // no more single-entry nodes: pick the best unvisited node to continue
        RDGraphNode best = 0;
        RDGraphNode best_parent = 0;
        usize best_edges = 0;

        const RDEgoBlock* vb;
        vect_each(vb, &ll->blocks) {
            if(!visited[rd_i_node2index(vb->node)]) continue;

            const RDEdgeVect* out =
                rd_i_graph_get_outgoing_edges(ll->graph, vb->node);
            const RDGraphEdge* e;
            vect_each(e, out) {
                if(visited[rd_i_node2index(e->dst)]) continue;

                RDEgoBlock* dst = _rd_ego_block(ll, e->dst);
                usize inc = vect_length(&dst->incoming);

                if(!best || inc < best_edges ||
                   (inc == best_edges && e->dst < best)) {
                    best = e->dst;
                    best_edges = inc;
                    best_parent = vb->node;
                }
            }
        }

        if(best) {
            RDEgoBlock* bp = _rd_ego_block(ll, best_parent);
            RDEgoBlock* dst = _rd_ego_block(ll, best);

            RDGraphNode* it;
            vect_each(it, &dst->incoming) {
                if(*it == best_parent) {
                    vect_del(&dst->incoming, vect_idx(&dst->incoming, it), 1);
                    break;
                }
            }

            vect_push(&bp->new_outgoing, best);
            visited[rd_i_node2index(best)] = true;
            vect_push(&queue, best);
            changed = true;
        }
    }

    rd_free(visited);
    vect_destroy(&queue);
}

// ---------------------------------------------------------------------------
// Step 3: compute_layout (recursive)
// Assigns col / row / colcount / rowcount to every block.
// Identical to layered.c's algorithm, it only ever reads/writes through
// the RDEgoBlock* it's given and recurses into ->new_outgoing, so it works
// unchanged whether that block is a real node or the synthetic vroot.
// ---------------------------------------------------------------------------

static void _rd_ego_adjust_layout(RDEgoLayout* ll, RDEgoBlock* block, int col,
                                  int row) {
    block->col += col;
    block->row += row;

    const RDGraphNode* n;
    vect_each(n, &block->new_outgoing)
        _rd_ego_adjust_layout(ll, _rd_ego_block(ll, *n), col, row);
}

static void _rd_ego_compute_layout(RDEgoLayout* ll, RDEgoBlock* block) {
    int col = 0;
    int rowcount = 1;
    int childcol = 0;
    bool single = vect_length(&block->new_outgoing) == 1;

    // recurse first
    const RDGraphNode* n;
    vect_each(n, &block->new_outgoing) {
        _rd_ego_compute_layout(ll, _rd_ego_block(ll, *n));
        RDEgoBlock* child = _rd_ego_block(ll, *n);
        if((child->rowcount + 1) > rowcount) rowcount = child->rowcount + 1;
        childcol = child->col;
    }

    if(ll->kind != RD_EGO_LAYOUT_WIDE &&
       vect_length(&block->new_outgoing) == 2) {

        RDEgoBlock* left = _rd_ego_block(ll, block->new_outgoing.data[0]);
        RDEgoBlock* right = _rd_ego_block(ll, block->new_outgoing.data[1]);

        if(vect_length(&left->new_outgoing) == 0) {
            left->col = right->col - 2;
            int add = left->col < 0 ? -left->col : 0;
            _rd_ego_adjust_layout(ll, right, add, 1);
            _rd_ego_adjust_layout(ll, left, add, 1);
            col = right->colcount + add;
        }
        else if(vect_length(&right->new_outgoing) == 0) {
            _rd_ego_adjust_layout(ll, left, 0, 1);
            _rd_ego_adjust_layout(ll, right, left->col + 2, 1);
            col = left->colcount > (right->col + 2) ? left->colcount
                                                    : (right->col + 2);
        }
        else {
            _rd_ego_adjust_layout(ll, left, 0, 1);
            _rd_ego_adjust_layout(ll, right, left->colcount, 1);
            col = left->colcount + right->colcount;
        }

        block->colcount = col > 2 ? col : 2;

        if(ll->kind == RD_EGO_LAYOUT_MEDIUM)
            block->col = (left->col + right->col) / 2;
        else
            block->col = single ? childcol : (col - 2) / 2;
    }
    else {
        vect_each(n, &block->new_outgoing) {
            RDEgoBlock* child = _rd_ego_block(ll, *n);
            _rd_ego_adjust_layout(ll, child, col, 1);
            col += child->colcount;
        }

        if(col >= 2) {
            block->col = single ? childcol : (col - 2) / 2;
            block->colcount = col;
        }
        else {
            block->col = 0;
            block->colcount = 2;
        }
    }

    block->row = 0;
    block->rowcount = rowcount;
}

// ---------------------------------------------------------------------------
// Step 4: prepare_edge_routing
// Allocate and initialise the three flat grids.
//
// DIFFERENCE FROM layered.c: grid size comes from ll->vroot's rowcount /
// colcount, not from the real query node's vroot is the actual top of
// the compute_layout recursion here, so only its counts reflect the whole
// graph's extent.
// The real query node's own counts only reflect its own subtree, same as any
// other non-top block.
// ---------------------------------------------------------------------------

static void _rd_ego_prepare_edge_routing(RDEgoLayout* ll) {
    ll->rowcount = ll->vroot.rowcount;
    ll->colcount = ll->vroot.colcount;

    usize cells = (usize)(ll->rowcount + 1) * (usize)(ll->colcount + 1);

    ll->horiz_lanes = rd_alloc0(cells, sizeof(int));
    ll->vert_lanes = rd_alloc0(cells, sizeof(int));
    ll->edge_valid = rd_alloc(cells * sizeof(bool));
    assert(ll->horiz_lanes && ll->vert_lanes && ll->edge_valid);

    // all cells valid by default
    memset(ll->edge_valid, 1, cells * sizeof(bool));

    // mark cells occupied by (real) nodes as invalid for vertical routing
    const RDEgoBlock* b;
    vect_each(b, &ll->blocks) _rd_ego_valid_set(ll, b->row, b->col + 1, false);
}

// ---------------------------------------------------------------------------
// Step 5: edge routing helpers
// ---------------------------------------------------------------------------

// Find the lowest lane index that is free across all cells in [mincol..maxcol]
// for the given row in horiz_lanes, then claim it.
static int _rd_ego_find_horiz_index(RDEgoLayout* ll, int row, int mincol,
                                    int maxcol) {
    int i = 0;
    while(true) {
        bool valid = true;
        for(int col = mincol; col <= maxcol; col++) {
            if(_rd_ego_horiz(ll, row, col) > i) {
                valid = false;
                break;
            }
        }
        if(valid) break;
        i++;
    }
    for(int col = mincol; col <= maxcol; col++) {
        if(_rd_ego_horiz(ll, row, col) <= i)
            _rd_ego_horiz_set(ll, row, col, i + 1);
    }
    return i;
}

// Find the lowest lane index that is free across all cells in [minrow..maxrow]
// for the given col in vert_lanes, then claim it.
static int _rd_ego_find_vert_index(RDEgoLayout* ll, int col, int minrow,
                                   int maxrow) {
    int i = 0;
    while(true) {
        bool valid = true;
        for(int row = minrow; row <= maxrow; row++) {
            if(_rd_ego_vert(ll, row, col) > i) {
                valid = false;
                break;
            }
        }
        if(valid) break;
        i++;
    }
    for(int row = minrow; row <= maxrow; row++) {
        if(_rd_ego_vert(ll, row, col) <= i)
            _rd_ego_vert_set(ll, row, col, i + 1);
    }
    return i;
}

// ---------------------------------------------------------------------------
// Step 5: route_edge
// Finds a non-overlapping grid path from start to end and stores waypoints.
// Identical to layered.c: already direction-agnostic (min/maxrow are
// computed either way), which is exactly what lets edges whose source and
// destination ranks go in either order ancestor->root and
// root->descendant alike route through the same code path without special
// casing.
// ---------------------------------------------------------------------------

static RDEgoEdge _rd_ego_route_edge(RDEgoLayout* ll, RDEgoBlock* start,
                                    RDEgoBlock* end) {
    RDEgoEdge edge = {
        .src_block = start,
        .dst_block = end,
    };

    // claim initial vertical lane leaving the source block
    int i = _rd_ego_vert(ll, start->row + 1, start->col + 1);
    _rd_ego_vert_set(ll, start->row + 1, start->col + 1, i + 1);
    _rd_ego_edge_add_point(&edge, start->row + 1, start->col + 1, 0);
    edge.start_index = i;

    bool horiz = false;

    // determine vertical range to travel
    int minrow, maxrow;
    if(end->row < (start->row + 1)) {
        minrow = end->row;
        maxrow = start->row + 1;
    }
    else {
        minrow = start->row + 1;
        maxrow = end->row;
    }

    // find a valid column for the vertical segment
    int col = start->col + 1;

    if(minrow != maxrow) {
        // check if preferred column (start->col+1) is clear
        bool col_ok = true;
        if(col < 0 || col > ll->colcount) {
            col_ok = false;
        }
        else {
            for(int row = minrow; row < maxrow; row++) {
                if(!_rd_ego_valid(ll, row, col)) {
                    col_ok = false;
                    break;
                }
            }
        }

        if(!col_ok) {
            // try end column
            bool end_ok = true;
            int ecol = end->col + 1;
            if(ecol < 0 || ecol > ll->colcount) {
                end_ok = false;
            }
            else {
                for(int row = minrow; row < maxrow; row++) {
                    if(!_rd_ego_valid(ll, row, ecol)) {
                        end_ok = false;
                        break;
                    }
                }
            }

            if(end_ok) {
                col = ecol;
            }
            else {
                // spiral outward from start->col+1
                int ofs = 1;
                while(true) {
                    int try_col = start->col + 1 - ofs;
                    bool ok = true;
                    if(try_col >= 0 && try_col <= ll->colcount) {
                        for(int row = minrow; row < maxrow; row++) {
                            if(!_rd_ego_valid(ll, row, try_col)) {
                                ok = false;
                                break;
                            }
                        }
                        if(ok) {
                            col = try_col;
                            break;
                        }
                    }
                    try_col = start->col + 1 + ofs;
                    ok = true;
                    if(try_col >= 0 && try_col <= ll->colcount) {
                        for(int row = minrow; row < maxrow; row++) {
                            if(!_rd_ego_valid(ll, row, try_col)) {
                                ok = false;
                                break;
                            }
                        }
                        if(ok) {
                            col = try_col;
                            break;
                        }
                    }
                    ofs++;
                }
            }
        }
    }

    // horizontal move to reach the chosen column
    if(col != (start->col + 1)) {
        int mincol = col < (start->col + 1) ? col : (start->col + 1);
        int maxcol = col < (start->col + 1) ? (start->col + 1) : col;
        int idx = _rd_ego_find_horiz_index(ll, start->row + 1, mincol, maxcol);
        _rd_ego_edge_add_point(&edge, start->row + 1, col, idx);
        horiz = true;
    }

    // vertical move to reach the target row
    if(end->row != (start->row + 1)) {
        if(col == (start->col + 1)) {
            // temporarily "unmark" the initial slot to avoid counting it twice
            int cur = _rd_ego_vert(ll, start->row + 1, start->col + 1);
            _rd_ego_vert_set(ll, start->row + 1, start->col + 1,
                             cur > 0 ? cur - 1 : 0);
        }
        int idx = _rd_ego_find_vert_index(ll, col, minrow, maxrow);
        if(col == (start->col + 1)) {
            // re-claim the initial slot
            int cur = _rd_ego_vert(ll, start->row + 1, start->col + 1);
            _rd_ego_vert_set(ll, start->row + 1, start->col + 1, cur + 1);
            edge.start_index = idx;
        }
        _rd_ego_edge_add_point(&edge, end->row, col, idx);
        horiz = false;
    }

    // horizontal move to reach the target column
    if(col != (end->col + 1)) {
        int mincol = col < (end->col + 1) ? col : (end->col + 1);
        int maxcol = col < (end->col + 1) ? (end->col + 1) : col;
        int idx = _rd_ego_find_horiz_index(ll, end->row, mincol, maxcol);
        _rd_ego_edge_add_point(&edge, end->row, end->col + 1, idx);
        horiz = true;
    }

    // if last segment was horizontal, claim the final vertical arrival lane
    if(horiz) {
        int idx = _rd_ego_find_vert_index(ll, end->col + 1, end->row, end->row);
        usize len = vect_length(&edge.points);
        if(len > 0) vect_at(&edge.points, len - 1)->index = idx;
    }

    return edge;
}

// ---------------------------------------------------------------------------
// Step 5: perform_edge_routing
// Route every (real) edge in BFS order and store results on the source
// block.
// ---------------------------------------------------------------------------

static void _rd_ego_perform_edge_routing(RDEgoLayout* ll) {
    const RDGraphNode* n;
    vect_each(n, &ll->block_order) {
        RDEgoBlock* block = _rd_ego_block(ll, *n);
        const RDEdgeVect* out = rd_i_graph_get_outgoing_edges(ll->graph, *n);
        const RDGraphEdge* e;
        vect_each(e, out) {
            RDEgoBlock* dst = _rd_ego_block(ll, e->dst);
            RDEgoEdge edge = _rd_ego_route_edge(ll, block, dst);
            vect_push(&block->routed_edges, edge);
        }
    }
}

// ---------------------------------------------------------------------------
// Step 6: compute_edge_count
// Walk every grid cell and record the max lane count per row / col.
// ---------------------------------------------------------------------------

static void _rd_ego_compute_edge_count(RDEgoLayout* ll) {
    int rows = ll->rowcount + 1;
    int cols = ll->colcount + 1;

    ll->col_edge_count = rd_alloc0((usize)cols, sizeof(int));
    ll->row_edge_count = rd_alloc0((usize)rows, sizeof(int));
    assert(ll->col_edge_count && ll->row_edge_count);

    for(int row = 0; row < rows; row++) {
        for(int col = 0; col < cols; col++) {
            int h = _rd_ego_horiz(ll, row, col);
            int v = _rd_ego_vert(ll, row, col);
            if(h > ll->row_edge_count[row]) ll->row_edge_count[row] = h;
            if(v > ll->col_edge_count[col]) ll->col_edge_count[col] = v;
        }
    }
}

// ---------------------------------------------------------------------------
// Step 7: compute_row_column_sizes
// Determine pixel width of each column and pixel height of each row.
// ---------------------------------------------------------------------------

static void _rd_ego_compute_row_col_sizes(RDEgoLayout* ll) {
    int rows = ll->rowcount + 1;
    int cols = ll->colcount + 1;

    ll->col_width = rd_alloc0((usize)cols, sizeof(int));
    ll->row_height = rd_alloc0((usize)rows, sizeof(int));
    assert(ll->col_width && ll->row_height);

    const RDEgoBlock* b;
    vect_each(b, &ll->blocks) {
        int hw = b->width / 2;
        if(hw > ll->col_width[b->col]) ll->col_width[b->col] = hw;
        if(hw > ll->col_width[b->col + 1]) ll->col_width[b->col + 1] = hw;
        if(b->height > ll->row_height[b->row])
            ll->row_height[b->row] = b->height;
    }
}

// ---------------------------------------------------------------------------
// Step 8: compute_row_column_positions
// Convert sizes + edge counts into pixel positions.
// ---------------------------------------------------------------------------

static void _rd_ego_compute_row_col_positions(RDEgoLayout* ll) {
    int rows = ll->rowcount + 1;
    int cols = ll->colcount + 1;

    ll->col_x = rd_alloc0((usize)ll->colcount, sizeof(int));
    ll->row_y = rd_alloc0((usize)ll->rowcount, sizeof(int));
    ll->col_edge_x = rd_alloc0((usize)cols, sizeof(int));
    ll->row_edge_y = rd_alloc0((usize)rows, sizeof(int));
    assert(ll->col_x && ll->row_y && ll->col_edge_x && ll->row_edge_y);

    int x = RD_EGO_PADDING;
    for(int i = 0; i < ll->colcount; i++) {
        ll->col_edge_x[i] = x;
        x += (RD_EGO_PADDING_DIV2 * ll->col_edge_count[i]);
        ll->col_x[i] = x;
        x += ll->col_width[i];
    }

    int y = RD_EGO_PADDING;
    for(int i = 0; i < ll->rowcount; i++) {
        ll->row_edge_y[i] = y;
        y += (RD_EGO_PADDING_DIV2 * ll->row_edge_count[i]);
        ll->row_y[i] = y;
        y += ll->row_height[i];
    }

    ll->col_edge_x[ll->colcount] = x;
    ll->row_edge_y[ll->rowcount] = y;

    rd_graph_set_area_width(ll->graph, x + RD_EGO_PADDING +
                                           (RD_EGO_PADDING_DIV2 *
                                            ll->col_edge_count[ll->colcount]));

    rd_graph_set_area_height(ll->graph, y + RD_EGO_PADDING +
                                            (RD_EGO_PADDING_DIV2 *
                                             ll->row_edge_count[ll->rowcount]));
}

// ---------------------------------------------------------------------------
// Step 9: compute_node_positions
// Place each (real) block in pixel space.
// ---------------------------------------------------------------------------

static void _rd_ego_compute_node_positions(RDEgoLayout* ll) {
    RDEgoBlock* b;
    // iterate by pointer since we need to write back x/y
    for(usize i = 0; i < vect_length(&ll->blocks); i++) {
        b = &ll->blocks.data[i];

        float cx =
            ((float)(ll->col_x[b->col] + ll->col_width[b->col]) +
             (RD_EGO_PADDING_DIV4 * (float)ll->col_edge_count[b->col + 1])) -
            ((float)(b->width) / 2.0F);

        float right_bound =
            ((float)(ll->col_x[b->col] + ll->col_width[b->col] +
                     ll->col_width[b->col + 1]) +
             (RD_EGO_PADDING_DIV2 * (float)ll->col_edge_count[b->col + 1]));

        if((cx + (float)b->width) > right_bound)
            cx = right_bound - (float)b->width;

        b->x = cx;
        b->y = (float)(ll->row_y[b->row] + RD_EGO_PADDING);

        rd_graph_set_node_x(ll->graph, b->node, (int)b->x);
        rd_graph_set_node_y(ll->graph, b->node, (int)b->y);
    }
}

// ---------------------------------------------------------------------------
// Step 10: precompute_edge_coordinates
// Convert grid waypoints to pixel polylines and write back to the graph.
// ---------------------------------------------------------------------------

static void _rd_ego_precompute_edge_coords(RDEgoLayout* ll) {
    for(usize bi = 0; bi < vect_length(&ll->blocks); bi++) {
        RDEgoBlock* block = &ll->blocks.data[bi];

        for(usize ei = 0; ei < vect_length(&block->routed_edges); ei++) {
            RDEgoEdge* edge = &block->routed_edges.data[ei];
            if(vect_length(&edge->points) == 0) continue;

            RDEgoPoint* first = vect_at(&edge->points, 0);
            int scol = first->col;
            int lidx = edge->start_index;

            RDGraphPoint lastpt = {
                .x = ll->col_edge_x[scol] + (RD_EGO_PADDING_DIV2 * lidx),
                .y = rd_graph_get_node_y(ll->graph, block->node) +
                     rd_graph_get_node_height(ll->graph, block->node) -
                     RD_EGO_NODE_PADDING,
            };

            vect_push(&edge->routes, lastpt);

            const RDEgoPoint* pt;
            vect_each(pt, &edge->points) {
                int endrow = pt->row;
                int endcol = pt->col;
                int eidx = pt->index;

                RDGraphPoint newpt;
                if(scol == endcol)
                    newpt = (RDGraphPoint){
                        .x = lastpt.x,
                        .y = ll->row_edge_y[endrow] +
                             (RD_EGO_PADDING_DIV2 * eidx) + 4,
                    };
                else
                    newpt = (RDGraphPoint){
                        .x = ll->col_edge_x[endcol] +
                             (RD_EGO_PADDING_DIV2 * eidx),
                        .y = lastpt.y,
                    };

                vect_push(&edge->routes, newpt);
                lastpt = newpt;
                scol = endcol;
            }

            // final segment arriving at destination node
            RDGraphPoint tip = {
                .x = lastpt.x,
                .y = rd_graph_get_node_y(ll->graph, edge->dst_block->node) - 1,
            };
            vect_push(&edge->routes, tip);

            // arrowhead: 3-point triangle
            vect_push(&edge->arrow, ((RDGraphPoint){tip.x - 3, tip.y - 6}));
            vect_push(&edge->arrow, ((RDGraphPoint){tip.x + 3, tip.y - 6}));
            vect_push(&edge->arrow, tip);

            // write routes and arrow back to the graph edge
            const RDGraphEdge* ge = rd_graph_find_edge(ll->graph, block->node,
                                                       edge->dst_block->node);
            if(!ge) continue;

            rd_graph_set_edge_routes(ll->graph, ge, edge->routes.data,
                                     vect_length(&edge->routes));
            rd_graph_set_edge_arrow(ll->graph, ge, edge->arrow.data,
                                    vect_length(&edge->arrow));
        }
    }
}

// ---------------------------------------------------------------------------
// Cleanup: free everything owned by the layout state
// ---------------------------------------------------------------------------

static void _rd_ego_destroy(RDEgoLayout* ll) {
    // free per-block dynamic data
    for(usize i = 0; i < vect_length(&ll->blocks); i++) {
        RDEgoBlock* b = &ll->blocks.data[i];
        vect_destroy(&b->incoming);
        vect_destroy(&b->new_outgoing);

        for(usize j = 0; j < vect_length(&b->routed_edges); j++) {
            RDEgoEdge* e = &b->routed_edges.data[j];
            vect_destroy(&e->points);
            vect_destroy(&e->routes);
            vect_destroy(&e->arrow);
        }
        vect_destroy(&b->routed_edges);
    }
    vect_destroy(&ll->blocks);
    vect_destroy(&ll->block_order);

    // vroot lives outside ll->blocks (it's not a real node), so its own
    // dynamic fields aren't freed by the loop above
    vect_destroy(&ll->vroot.incoming); // unused, always empty
    vect_destroy(&ll->vroot.new_outgoing);
    vect_destroy(&ll->vroot.routed_edges); // unused, always empty

    // free flat grids
    rd_free(ll->horiz_lanes);
    rd_free(ll->vert_lanes);
    rd_free(ll->edge_valid);

    // free sizing arrays
    rd_free(ll->col_width);
    rd_free(ll->row_height);
    rd_free(ll->col_x);
    rd_free(ll->row_y);
    rd_free(ll->col_edge_x);
    rd_free(ll->row_edge_y);
    rd_free(ll->col_edge_count);
    rd_free(ll->row_edge_count);
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

bool rd_graph_compute_ego(RDGraph* self, RDEgoLayoutKind kind) {
    if(!self || !self->root) return false;

    RDEgoLayout ll = {
        .graph = self,
        .kind = kind,
    };

    usize node_count = vect_length(&self->nodes);
    vect_reserve(&ll.blocks, node_count); // stable pointers throughout

    _rd_ego_create_blocks(&ll);
    _rd_ego_make_acyclic(&ll);

    _rd_ego_compute_layout(&ll, &ll.vroot);

    _rd_ego_prepare_edge_routing(&ll);
    _rd_ego_perform_edge_routing(&ll);
    _rd_ego_compute_edge_count(&ll);
    _rd_ego_compute_row_col_sizes(&ll);
    _rd_ego_compute_row_col_positions(&ll);
    _rd_ego_compute_node_positions(&ll);
    _rd_ego_precompute_edge_coords(&ll);

    _rd_ego_destroy(&ll);
    return true;
}
