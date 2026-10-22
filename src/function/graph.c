#include "graphing/graph.h"
#include "core/context.h"
#include "core/worker.h"
#include "function/chunk.h"
#include "support/containers.h"
#include <inttypes.h>
#include <redasm/allocator.h>
#include <redasm/function/graph.h>

typedef struct RDFunctionGraphNode {
    RDChunk chunk;
    RDGraphNode id;
    bool noret_end;
} RDFunctionGraphNode;

typedef struct RDFunctionGraph {
    RDGraph base;
    u32 head_gen;
    RDRelAddress rel_entry;
    RDCharVect fmt_buf;
    RDChunkVect chunk_buf;

    struct {
        RDFunctionGraphNode* data;
        usize length;
        usize capacity;
    } nodes;
} RDFunctionGraph;

static int _rd_functiongraph_start_kcmp(const void* key, const void* item) {
    RDRelAddress k = *(const RDRelAddress*)key;
    const RDFunctionGraphNode* n = (const RDFunctionGraphNode*)item;
    return (k > n->chunk.start) - (k < n->chunk.start);
}

static void _rd_functiongraph_destroy(RDGraph* base) {
    RDFunctionGraph* self = (RDFunctionGraph*)base;
    vect_destroy(&self->nodes);
    vect_destroy(&self->chunk_buf);
    vect_destroy(&self->fmt_buf);
}

static RDGraphNode _rd_functiongraph_add(RDFunctionGraph* self,
                                         const RDChunk* c) {
    RDGraphNode n = rd_graph_add_node(&self->base);
    vect_push(&self->nodes, (RDFunctionGraphNode){.chunk = *c, .id = n});
    rd_graph_set_data(&self->base, n, (RDNodeData)vect_length(&self->nodes));
    return n;
}

static RDGraphNode _rd_functiongraph_find(const RDFunctionGraph* self,
                                          RDRelAddress start) {
    usize i = vect_bsearch(&self->nodes, &start, _rd_functiongraph_start_kcmp);
    return i != vect_length(&self->nodes) ? vect_at(&self->nodes, i)->id : 0;
}

static RDFunctionGraphNode* _rd_functiongraph_node(const RDFunctionGraph* self,
                                                   RDGraphNode n) {
    if(!self || !n) return NULL;

    usize idx = (usize)rd_graph_get_data(&self->base, n);
    return idx && idx <= vect_length(&self->nodes)
               ? vect_at(&self->nodes, idx - 1)
               : NULL;
}

static u64 _rd_functiongraph_weight(const RDGraph* g, RDGraphNode n, void* ud) {
    RD_UNUSED(ud);
    const RDFunctionGraphNode* d =
        _rd_functiongraph_node((const RDFunctionGraph*)g, n);
    assert(d && "node without chunk");
    return d ? d->chunk.start : 0;
}

static const char* _rd_functiongraph_dot_props(const RDGraph* g, RDGraphNode n,
                                               void* ud) {
    RDFunctionGraph* self = (RDFunctionGraph*)ud;
    const RDFunctionGraphNode* d =
        _rd_functiongraph_node((const RDFunctionGraph*)g, n);
    return rd_i_format(&self->fmt_buf, "[label=\"0x%" PRIX64 "\", noret=%s]",
                       rd_i_abs(self->base.context, d->chunk.start),
                       d->noret_end ? "true" : "false");
}

static void _rd_functiongraph_clear(RDFunctionGraph* self) {
    rd_i_graph_clear(&self->base);
    vect_clear(&self->nodes);
    self->base.gen = 0;
    self->head_gen = 0;
}

RDFunctionGraph* rd_functiongraph_create(RDContext* ctx) {
    return rd_graph_create_as(RDFunctionGraph, ctx, _rd_functiongraph_destroy);
}

bool rd_functiongraph_populate(RDFunctionGraph* self, RDAddress entry) {
    RDContext* ctx = self->base.context;

    if(ctx->engine.step < RD_WS_DONE) {
        RD_LOG_WARN("function graphs are available after analysis has "
                    "finished (current step: %s)",
                    rd_i_worker_get_step_name(ctx->engine.step));
        return false;
    }

    _rd_functiongraph_clear(self);

    if(!rd_is_address(ctx, entry)) return false;

    RDRelAddress rel_entry = rd_i_rel(ctx, entry);

    const RDChunk* head = rd_i_chunks_find_head_rel(ctx, rel_entry);
    if(!head) return false;

    rd_i_chunks_collect(ctx, entry, &self->chunk_buf);

    RDGraph* g = &self->base;
    RDGraphNode root = 0;

    const RDChunk* c;
    vect_each(c, &self->chunk_buf) {
        RDGraphNode n = _rd_functiongraph_add(self, c);
        if(!root && rel_entry == c->start) root = n;
    }

    if(!root) {
        _rd_functiongraph_clear(self);
        return false;
    }

    self->rel_entry = rel_entry;
    self->head_gen = head->gen;        // staleness check
    self->base.gen = ++ctx->graph_gen; // identity check

    rd_graph_set_root(g, root);

    RDChunkSuccVect succ = {0};
    RDXRefVect refs = {0};

    vect_each(c, &self->chunk_buf) {
        RDGraphNode src = _rd_functiongraph_find(self, c->start);
        bool noret_end;
        rd_i_chunk_successors(ctx, c, &succ, &refs, &noret_end);

        RDFunctionGraphNode* fg_node = _rd_functiongraph_node(self, src);
        assert(fg_node);
        if(!fg_node) continue;

        fg_node->noret_end = noret_end;

        const RDChunkSucc* s;
        vect_each(s, &succ) {
            if(!rd_is_address(ctx, s->address)) continue;

            RDGraphNode dst =
                _rd_functiongraph_find(self, rd_i_rel(ctx, s->address));
            if(!dst) continue; // not a chunk of this function

            RDGraphEdge e = rd_graph_add_edge(g, src, dst);

            if(s->kind == RD_SUCC_TRUE) {
                rd_graph_set_edge_color(g, &e,
                                        rd_get_theme_color(RD_THEME_SUCCESS));
            }
            else if(s->kind == RD_SUCC_FALSE) {
                rd_graph_set_edge_color(g, &e,
                                        rd_get_theme_color(RD_THEME_FAIL));
            }
        }
    }

    vect_destroy(&refs);
    vect_destroy(&succ);

    if(!rd_graph_order(g, _rd_functiongraph_weight, NULL)) {
        rd_functiongraph_destroy(self);
        panic("ordering failed at 0x%" PRIX64, entry);
        return NULL;
    }

    return true;
}

void rd_functiongraph_destroy(RDFunctionGraph* self) {
    rd_graph_destroy((RDGraph*)self); // let 'destroy' handle deallocation
}

bool rd_functiongraph_get_chunk_start(const RDFunctionGraph* self,
                                      RDGraphNode n, RDAddress* start_addr) {
    const RDFunctionGraphNode* node = _rd_functiongraph_node(self, n);
    if(!node) return false;

    if(start_addr)
        *start_addr = rd_i_abs(self->base.context, node->chunk.start);
    return true;
}

bool rd_functiongraph_get_chunk_end(const RDFunctionGraph* self, RDGraphNode n,
                                    RDAddress* end_addr) {
    const RDFunctionGraphNode* node = _rd_functiongraph_node(self, n);
    if(!node) return false;

    if(end_addr) *end_addr = rd_i_abs(self->base.context, node->chunk.end);
    return true;
}

bool rd_functiongraph_is_chunk_noret(const RDFunctionGraph* self, RDGraphNode n,
                                     bool* is_noret) {
    const RDFunctionGraphNode* node = _rd_functiongraph_node(self, n);
    if(!node) return false;

    if(is_noret) *is_noret = node->noret_end;
    return true;
}

bool rd_functiongraph_get_address(const RDFunctionGraph* self,
                                  RDAddress* address) {
    if(!self || rd_graph_is_empty(&self->base)) return false;

    if(address) *address = rd_i_abs(self->base.context, self->rel_entry);
    return true;
}

bool rd_functiongraph_is_current(const RDFunctionGraph* self) {
    if(!self || rd_graph_is_empty(&self->base)) return false;

    const RDChunk* head =
        rd_i_chunks_find_head_rel(self->base.context, self->rel_entry);
    return head && head->gen == self->head_gen;
}

const char* rd_functiongraph_generate_dot(const RDFunctionGraph* self) {
    return rd_graph_generate_dot(&self->base, _rd_functiongraph_dot_props,
                                 (void*)self);
}

const char* rd_functiongraph_generate_dot_layout(const RDFunctionGraph* self) {
    return rd_graph_generate_dot(&self->base, NULL, NULL);
}

u32 rd_functiongraph_get_hash(const RDFunctionGraph* self) {
    return rd_graph_get_hash(&self->base, _rd_functiongraph_dot_props,
                             (void*)self);
}

u32 rd_functiongraph_get_hash_layout(const RDFunctionGraph* self) {
    return rd_graph_get_hash(&self->base, NULL, NULL);
}
