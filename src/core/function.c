#include "function.h"
#include "core/context.h"
#include "graphs/graph.h"
#include "io/flags.h"
#include "io/flagsbuffer.h"
#include "support/containers.h"
#include <redasm/function.h>
#include <stdlib.h>

typedef struct RDFunctionWorkItem {
    RDAddress address;
    RDGraphNode node;
} RDFunctionWorkItem;

typedef struct RDFunctionWorkVect {
    RDFunctionWorkItem* data;
    usize length;
    usize capacity;
} RDFunctionWorkVect;

static int _rd_functionchunk_cmp(const void* arg1, const void* arg2) {
    const RDFunctionChunk* chunk1 = *(const RDFunctionChunk**)arg1;
    const RDFunctionChunk* chunk2 = *(const RDFunctionChunk**)arg2;
    if(chunk1->start < chunk2->start) return -1;
    if(chunk1->start > chunk2->start) return 1;
    return 0;
}

static RDGraphNode _rd_function_get_or_add_block(RDGraph* g, RDAddress start,
                                                 RDAddress ep,
                                                 RDFunctionWorkVect* w,
                                                 RDFunctionChunkVect* chunks) {
    const RDNodeVect* nodes = rd_i_graph_get_nodes(g);
    const RDGraphNode* it;

    vect_each(it, nodes) {
        const RDFunctionChunk* b =
            (const RDFunctionChunk*)rd_graph_get_data(g, *it);
        if(b->start == start) return *it;
    }

    RDFunctionChunk* b = calloc(1, sizeof(*b));
    b->func_address = ep;
    b->start = start;
    vect_push(chunks, b);

    RDGraphNode n = rd_graph_add_node(g);
    rd_graph_set_data(g, n, (RDNodeData)b);

    vect_push(w, (RDFunctionWorkItem){.address = start, .node = n});
    return n;
}

void rd_i_function_build_graph(RDFunction* self, RDFunctionChunkVect* chunks) {
    RDContext* ctx = self->context;
    RDGraph* g = rd_graph_create();
    RDFunctionWorkVect w = {0};
    RDXRefVect refs = {0};

    // set function entry
    RDGraphNode root = _rd_function_get_or_add_block(g, self->address,
                                                     self->address, &w, chunks);
    rd_graph_set_root(g, root);

    while(!vect_is_empty(&w)) {
        RDFunctionWorkItem wi = vect_pop_back(&w);
        RDAddress addr = wi.address;
        RDGraphNode src = wi.node;

        const RDSegmentFull* seg = rd_i_find_segment(ctx, addr);
        if(!seg) continue;

        while(addr < seg->base.end_address) {
            usize idx = rd_i_address2index(seg, addr);
            RDFlags flags = rd_i_flagsbuffer_get(seg->flags, idx);

            // stop at non-code or tail bytes
            if(!rd_i_flags_has_code(flags) || rd_i_flags_has_tail(flags)) break;

            usize len = rd_i_flagsbuffer_get_range_length(seg->flags, idx);
            if(!len) break;

            RDAddress nextaddr = addr + len;

            if(rd_i_flags_has_jump(flags)) {
                rd_i_get_xrefs_from_type_ex(ctx, addr, RD_CR_JUMP, &refs);

                if(rd_i_flags_has_cond(flags)) {
                    // true edge(s): jump targets from refs
                    const RDXRef* r;
                    vect_each(r, &refs) {
                        RDGraphNode dst = _rd_function_get_or_add_block(
                            g, r->address, self->address, &w, chunks);
                        RDGraphEdge e = rd_graph_add_edge(g, src, dst);
                        const char* c = rd_get_theme_color(RD_THEME_SUCCESS);
                        rd_graph_set_edge_color(g, &e, c);
                    }

                    // false edge: fall-through
                    RDGraphNode dst = _rd_function_get_or_add_block(
                        g, nextaddr, self->address, &w, chunks);
                    RDGraphEdge e = rd_graph_add_edge(g, src, dst);
                    const char* c = rd_get_theme_color(RD_THEME_FAIL);
                    rd_graph_set_edge_color(g, &e, c);
                }
                else { // unconditional: single jump edge per target
                    const RDXRef* r;
                    vect_each(r, &refs) {
                        RDGraphNode dst = _rd_function_get_or_add_block(
                            g, r->address, self->address, &w, chunks);
                        rd_graph_add_edge(g, src, dst);
                    }
                }

                addr = nextaddr;
                break; // block ends at jump
            }

            // check next instruction
            if(nextaddr >= seg->base.end_address) {
                addr = nextaddr;
                break; // end of segment, block ends
            }

            usize nextidx = rd_i_address2index(seg, nextaddr);
            RDFlags nextflags = rd_i_flagsbuffer_get(seg->flags, nextidx);

            // ret, noret call, or end of reachable code
            // no outgoing edge
            if(!rd_i_flags_has_flow(nextflags)) {
                addr = nextaddr;
                break;
            }

            // someone references here: forced split, add flow edge
            if(rd_i_flags_has_xref_in(nextflags)) {
                RDGraphNode dst = _rd_function_get_or_add_block(
                    g, nextaddr, self->address, &w, chunks);
                rd_graph_add_edge(g, src, dst);
                addr = nextaddr;
                break;
            }

            addr = nextaddr;
        }

        // finalize block end address
        RDFunctionChunk* b = (RDFunctionChunk*)rd_graph_get_data(g, src);
        b->end = addr;
    }

    vect_destroy(&refs);
    vect_destroy(&w);
    self->graph = g;
}

void rd_i_functionchunk_sort(RDFunctionChunkVect* self) {
    vect_sort(self, _rd_functionchunk_cmp);
}

RDFunctionChunk* rd_i_function_get_chunk(const RDFunction* self,
                                         RDGraphNode n) {
    if(!self->graph) return NULL;
    return (RDFunctionChunk*)rd_graph_get_data(self->graph, n);
}

void rd_i_functionchunk_destroy(RDFunctionChunkVect* self) {
    RDFunctionChunk** chunk;
    vect_each(chunk, self) { free(*chunk); }
    vect_destroy(self);
}

RDFunction* rd_i_function_create(RDContext* ctx, RDAddress address) {
    RDFunction* self = malloc(sizeof(*self));

    *self = (RDFunction){
        .context = ctx,
        .address = address,
    };

    return self;
}

void rd_i_function_destroy(RDFunction* self) {
    if(!self) return;

    rd_graph_destroy(self->graph);
    free(self);
}

RDGraph* rd_function_get_graph(const RDFunction* self) { return self->graph; }

RDAddress rd_function_get_address(const RDFunction* self) {
    return self->address;
}

bool rd_function_get_chunk(const RDFunction* self, RDGraphNode n,
                           RDFunctionChunk* chunk) {
    if(!self->graph) return false;

    const RDFunctionChunk* c = rd_i_function_get_chunk(self, n);
    if(!c) return false;
    if(chunk) *chunk = *c;
    return true;
}

int rd_i_function_find_chunk_pred(const void* key, const void* item) {
    RDAddress address = *(const RDAddress*)key;
    const RDFunctionChunk* c = *(const RDFunctionChunk**)item;
    if(address < c->start) return -1;
    if(address >= c->end) return 1;
    return 0;
}

int rd_i_function_find_pred(const void* key, const void* item) {
    RDAddress address = *(const RDAddress*)key;
    const RDFunction* f = *(const RDFunction**)item;
    if(address < f->address) return -1;
    if(address > f->address) return 1;
    return 0;
}
