#include "callgraph.h"
#include "core/context.h"
#include "io/flagsbuffer.h"
#include <redasm/allocator.h>
#include <redasm/graph/graph.h>

typedef struct RDCallGraphNode {
    RDRelAddress func_rel_address;
} RDCallGraphNode;

static void _rd_callgraph_destroy(RDGraph* base) {
    RDCallGraph* self = (RDCallGraph*)base;

    const RDNodeVect* nodes = rd_i_graph_get_nodes(&self->base);

    const RDGraphNode* it;
    vect_each(it, nodes) {
        RDCallGraphNode* d =
            (RDCallGraphNode*)rd_graph_get_data(&self->base, *it);

        rd_free(d);
    }

    vect_destroy(&self->xrefs_buf);
    vect_destroy(&self->wl);
}

static bool _rd_callgraph_get_or_add_block(RDCallGraph* self,
                                           RDRelAddress address,
                                           RDGraphNode* out_n) {
    const RDNodeVect* nodes = rd_i_graph_get_nodes(&self->base);

    const RDGraphNode* it;
    vect_each(it, nodes) {
        const RDCallGraphNode* d =
            (const RDCallGraphNode*)rd_graph_get_data(&self->base, *it);

        if(d->func_rel_address == address) {
            *out_n = *it;
            return false;
        }
    }

    RDCallGraphNode* d = rd_alloc(sizeof(*d));
    d->func_rel_address = address;

    *out_n = rd_graph_add_node(&self->base);
    rd_graph_set_data(&self->base, *out_n, (RDNodeData)d);
    return true;
}

static void _rd_callgraph_explore_up(RDCallGraph* self, RDGraphNode child_node,
                                     const RDFunction* f) {
    RDContext* ctx = self->context;

    const RDXRefVect* xrefs = rd_i_db_get_xrefs_to(
        ctx, rd_i_abs(ctx, f->rel_address), RD_CR_CALL, &self->xrefs_buf);

    const RDXRef* xref;
    vect_each(xref, xrefs) {
        const RDFunction* caller = rd_i_find_function(ctx, xref->address);
        if(!caller) continue; // call site not (yet) claimed by any function

        RDGraphNode n;
        bool added =
            _rd_callgraph_get_or_add_block(self, caller->rel_address, &n);

        // caller -> callee, same orientation as down edges
        RDGraphEdge e = rd_graph_add_edge(&self->base, n, child_node);
        rd_graph_set_edge_color(&self->base, &e,
                                rd_get_theme_color(RD_THEME_FAIL));

        if(added) vect_push(&self->wl, n);
    }
}

static void _rd_callgraph_explore_down(RDCallGraph* self,
                                       RDGraphNode parent_node,
                                       const RDFunction* f) {
    const RDNodeVect* nodes = rd_i_graph_get_nodes_ordered(f->graph);
    RDContext* ctx = self->context;

    const RDGraphNode* it;
    vect_each(it, nodes) {
        const RDFunctionChunk* b =
            (const RDFunctionChunk*)rd_graph_get_data(f->graph, *it);

        const RDSegment* seg =
            rd_i_db_find_segment(ctx, rd_i_abs(ctx, b->start));
        if(!seg) continue;

        usize idx = b->start - seg->rel_start,
              end_idx = b->end - seg->rel_start;

        while(idx < end_idx) {
            if(rd_flagsbuffer_has_call(seg->flags, idx)) {
                RDAddress addr = rd_i_index2address(seg, idx);
                const RDXRefVect* xrefs = rd_i_db_get_xrefs_from(
                    ctx, addr, RD_CR_CALL, &self->xrefs_buf);

                const RDXRef* xref;
                vect_each(xref, xrefs) {
                    RDGraphNode n;

                    bool added = _rd_callgraph_get_or_add_block(
                        self, rd_i_rel(ctx, xref->address), &n);

                    RDGraphEdge e =
                        rd_graph_add_edge(&self->base, parent_node, n);
                    rd_graph_set_edge_color(
                        &self->base, &e, rd_get_theme_color(RD_THEME_SUCCESS));

                    if(added) vect_push(&self->wl, n);
                }
            }

            idx += rd_i_flagsbuffer_get_range_length(seg->flags, idx);
        }
    }
}

static void _rd_callgraph_walk_up(RDCallGraph* self, RDGraphNode start) {
    RDContext* ctx = self->context;
    vect_push(&self->wl, start);

    while(!vect_is_empty(&self->wl)) {
        RDGraphNode n = vect_pop_last(&self->wl);

        RDCallGraphNode* d =
            (RDCallGraphNode*)rd_graph_get_data(&self->base, n);

        const RDFunction* f =
            rd_i_get_function(ctx, rd_i_abs(ctx, d->func_rel_address));

        if(f) _rd_callgraph_explore_up(self, n, f);
    }
}

static void _rd_callgraph_walk_down(RDCallGraph* self, RDGraphNode start) {
    RDContext* ctx = self->context;
    vect_push(&self->wl, start);

    while(!vect_is_empty(&self->wl)) {
        RDGraphNode n = vect_pop_last(&self->wl);

        RDCallGraphNode* d =
            (RDCallGraphNode*)rd_graph_get_data(&self->base, n);

        const RDFunction* f =
            rd_i_get_function(ctx, rd_i_abs(ctx, d->func_rel_address));

        if(f && f->graph) _rd_callgraph_explore_down(self, n, f);
    }
}

RDCallGraph* rd_callgraph_create(RDContext* ctx, RDAddress address) {
    const RDFunction* f = rd_i_find_function(ctx, address);
    if(!f || !f->graph) return NULL;

    RDCallGraph* self = rd_graph_create_as(RDCallGraph, _rd_callgraph_destroy);
    self->context = ctx;

    RDGraphNode root;
    _rd_callgraph_get_or_add_block(self, f->rel_address, &root);
    rd_graph_set_root(&self->base, root);
    _rd_callgraph_walk_down(self, root);
    _rd_callgraph_walk_up(self, root);
    return self;
}

void rd_callgraph_destroy(RDCallGraph* self) {
    rd_graph_destroy((RDGraph*)self); // let 'destroy' handle deallocation
}

RDAddress rd_callgraph_get_address(const RDCallGraph* self, RDGraphNode n) {
    const RDCallGraphNode* d =
        (const RDCallGraphNode*)rd_graph_get_data(&self->base, n);
    return rd_i_abs(self->context, d->func_rel_address);
}
