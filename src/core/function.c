#include "function.h"
#include "core/context.h"
#include "graphs/graph.h"
#include "io/flags.h"
#include "io/flagsbuffer.h"
#include "support/containers.h"
#include "support/logging.h"
#include <inttypes.h>
#include <redasm/allocator.h>
#include <redasm/function.h>

typedef struct RDFunctionWorkItem {
    RDAddress address;
    RDGraphNode node;
} RDFunctionWorkItem;

typedef struct RDFunctionWorkVect {
    RDFunctionWorkItem* data;
    usize length;
    usize capacity;
} RDFunctionWorkVect;

static int _rd_functionchunk_cmp_pred(const void* arg1, const void* arg2) {
    const RDFunctionChunk* chunk1 = *(const RDFunctionChunk**)arg1;
    const RDFunctionChunk* chunk2 = *(const RDFunctionChunk**)arg2;
    if(chunk1->start < chunk2->start) return -1;
    if(chunk1->start > chunk2->start) return 1;
    return 0;
}

static bool _rd_functionchunks_del_if_pred(const void* key, const void* item) {
    RDAddress func_addr = *(const RDAddress*)key;
    RDFunctionChunk* chunk = *(RDFunctionChunk**)item;
    if(chunk->func_address != func_addr) return false;

    rd_free(chunk);
    return true;
}

static const char* _rd_function_dot_props(const RDGraph* g, RDGraphNode n,
                                          void* userdata) {
    RDFunction* self = (RDFunction*)userdata;
    const RDFunctionChunk* c = (const RDFunctionChunk*)rd_graph_get_data(g, n);
    assert(c);

    return rd_i_format(&self->fmt_buf, "[label=\"0x%" PRIx64 " (%zu)\"]",
                       c->start, c->n_instructions);
}

static RDGraphNode _rd_function_get_or_add_block(RDContext* ctx, RDGraph* g,
                                                 RDAddress start, RDAddress ep,
                                                 RDFunctionWorkVect* w,
                                                 RDFunctionChunkVect* chunks) {
    // don't create phantom blocks to invalid or non-executable segments
    const RDSegmentFull* seg = rd_i_db_find_segment(ctx, start);
    if(!seg || !(seg->base.perm & RD_SP_X)) return 0;

    const RDNodeVect* nodes = rd_i_graph_get_nodes(g);
    const RDGraphNode* it;

    vect_each(it, nodes) {
        const RDFunctionChunk* b =
            (const RDFunctionChunk*)rd_graph_get_data(g, *it);
        if(b->start == start) return *it;
    }

    RDFunctionChunk* b = rd_alloc0(1, sizeof(*b));
    b->func_address = ep;
    b->start = start;
    vect_push(chunks, b);

    RDGraphNode n = rd_graph_add_node(g);
    rd_graph_set_data(g, n, (RDNodeData)b);

    vect_push(w, (RDFunctionWorkItem){.address = start, .node = n});
    return n;
}

static void _rd_function_rebuild_graph(RDFunction* self,
                                       RDFunctionChunkVect* chunks) {
    RDContext* ctx = self->context;
    RDGraph* g = rd_graph_create();
    RDFunctionWorkVect w = {0};
    RDXRefVect refs = {0};

    self->n_instructions = 0;
    self->n_norets = 0;

    // set function entry
    RDGraphNode root = _rd_function_get_or_add_block(ctx, g, self->address,
                                                     self->address, &w, chunks);
    if(root) rd_graph_set_root(g, root);

    while(!vect_is_empty(&w)) {
        RDFunctionWorkItem wi = vect_pop_last(&w);
        RDAddress addr = wi.address;
        RDGraphNode src = wi.node;
        RDFunctionChunk* b = (RDFunctionChunk*)rd_graph_get_data(g, src);

        const RDSegmentFull* seg = rd_i_db_find_segment(ctx, addr);
        assert(seg);
        assert(seg->base.perm & RD_SP_X);

        bool has_noret = false;

        while(addr < seg->base.end_address) {
            usize idx = rd_i_address2index(seg, addr);
            RDFlags flags = rd_i_flagsbuffer_get(seg->flags, idx);

            // stop at non-code or tail bytes
            if(!rd_i_flags_has_code(flags) || rd_i_flags_has_tail(flags)) break;

            usize len = rd_i_flagsbuffer_get_range_length(seg->flags, idx);
            if(!len) break;

            self->n_instructions++;
            b->n_instructions++;

            RDAddress nextaddr = addr + len;

            if(rd_i_flags_has_jump(flags)) {
                rd_i_get_xrefs_from_ex(ctx, addr, RD_CR_JUMP, &refs);

                // consume all delay slot instructions into current block
                while(nextaddr < seg->base.end_address) {
                    usize nextidx = rd_i_address2index(seg, nextaddr);
                    RDFlags nextflags =
                        rd_i_flagsbuffer_get(seg->flags, nextidx);
                    if(!rd_i_flags_has_dslot(nextflags)) break;

                    usize slotlen =
                        rd_i_flagsbuffer_get_range_length(seg->flags, nextidx);

                    self->n_instructions++;
                    b->n_instructions++;

                    nextaddr += slotlen;
                }

                if(rd_i_flags_has_cond(flags)) {
                    // true edge(s): jump targets from refs
                    const RDXRef* r;
                    vect_each(r, &refs) {
                        RDGraphNode dst = _rd_function_get_or_add_block(
                            ctx, g, r->address, self->address, &w, chunks);

                        if(dst) {
                            RDGraphEdge e = rd_graph_add_edge(g, src, dst);
                            const char* c =
                                rd_get_theme_color(RD_THEME_SUCCESS);
                            rd_graph_set_edge_color(g, &e, c);
                        }
                    }

                    // false edge: fall-through
                    RDGraphNode dst = _rd_function_get_or_add_block(
                        ctx, g, nextaddr, self->address, &w, chunks);

                    if(dst) {
                        RDGraphEdge e = rd_graph_add_edge(g, src, dst);
                        const char* c = rd_get_theme_color(RD_THEME_FAIL);
                        rd_graph_set_edge_color(g, &e, c);
                    }
                }
                else { // unconditional: single jump edge per target
                    const RDXRef* r;
                    vect_each(r, &refs) {
                        RDGraphNode dst = _rd_function_get_or_add_block(
                            ctx, g, r->address, self->address, &w, chunks);

                        if(dst) rd_graph_add_edge(g, src, dst);
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

            if(!rd_i_flags_has_flow(nextflags)) {
                if(rd_i_flags_has_noret(flags)) {
                    self->n_norets++;
                    has_noret = true;
                }

                addr = nextaddr;
                break;
            }

            // someone references here: forced split, add flow edge
            if(rd_i_flags_has_xref_in(nextflags)) {
                RDGraphNode dst = _rd_function_get_or_add_block(
                    ctx, g, nextaddr, self->address, &w, chunks);
                if(dst) rd_graph_add_edge(g, src, dst);
                addr = nextaddr;
                break;
            }

            addr = nextaddr;
        }

        // finalize block end address
        b->end = addr;
        b->has_noret = has_noret;
    }

    vect_destroy(&refs);
    vect_destroy(&w);

    RDGraph* oldgraph = self->graph;
    self->graph = g;
    if(oldgraph) rd_graph_destroy(oldgraph);
}

const char* rd_i_function_to_str(const RDFunction* self, RDContext* ctx) {
    RDName n;
    if(!rd_i_get_name(ctx, self->address, false, &n)) return NULL;

    const RDTypeDef* func_def = rd_i_typedef_find(ctx, n.value);
    if(!func_def || func_def->kind != RD_TKIND_FUNC) return NULL;

    RDCharVect* t_buf = &ctx->type_buf;
    str_clear(&ctx->tdef_buf);

    const RDFunctionType* f_type = &func_def->func_;

    str_append(&ctx->tdef_buf, rd_i_type_to_str(&f_type->ret, t_buf));
    str_push(&ctx->tdef_buf, ' ');
    str_append(&ctx->tdef_buf, func_def->name);

    str_push(&ctx->tdef_buf, '(');

    const RDParam* arg;
    vect_each(arg, &f_type->args) {
        assert(arg->name);
        if(arg != vect_first(&f_type->args)) str_push(&ctx->tdef_buf, ',');

        str_append(&ctx->tdef_buf, rd_i_type_to_str(&arg->type, t_buf));
        str_push(&ctx->tdef_buf, ' ');
        str_append(&ctx->tdef_buf, arg->name);
    }

    str_push(&ctx->tdef_buf, ')');

    if(rd_i_function_is_noret(self)) str_append(&ctx->tdef_buf, " noreturn");

    assert(!vect_is_empty(&ctx->tdef_buf));
    return ctx->tdef_buf.data;
}

void rd_i_functionchunk_sort(RDFunctionChunkVect* self) {
    vect_sort(self, _rd_functionchunk_cmp_pred);
}

RDFunctionChunk* rd_i_function_get_chunk(const RDFunction* self,
                                         RDGraphNode n) {
    if(!self->graph) return NULL;
    return (RDFunctionChunk*)rd_graph_get_data(self->graph, n);
}

void rd_i_functionchunk_destroy(RDFunctionChunkVect* self) {
    RDFunctionChunk** chunk;
    vect_each(chunk, self) { rd_free(*chunk); }
    vect_destroy(self);
}

RDFunction* rd_i_function_create(RDContext* ctx, RDAddress address) {
    RDFunction* self = rd_alloc(sizeof(*self));

    *self = (RDFunction){
        .context = ctx,
        .address = address,
    };

    return self;
}

void rd_i_function_destroy(RDFunction* self) {
    if(!self) return;

    vect_destroy(&self->fmt_buf);
    rd_graph_destroy(self->graph);
    rd_free(self);
}

bool rd_i_function_is_noret(const RDFunction* self) {
    if(!self->n_norets) return false;

    const RDNodeVect* nodes = rd_i_graph_get_nodes(self->graph);
    if(vect_is_empty(nodes)) return false;

    RDGraphNode* n;
    vect_each(n, nodes) {
        const RDEdgeVect* outgoing =
            rd_i_graph_get_outgoing_edges(self->graph, *n);

        if(!vect_is_empty(outgoing)) continue;

        const RDFunctionChunk* chunk = rd_i_function_get_chunk(self, *n);
        if(!chunk->has_noret) return false;
    }

    return true;
}

usize rd_i_function_get_terminal_count(const RDFunction* self) {
    const RDNodeVect* nodes = rd_i_graph_get_nodes(self->graph);

    usize res = 0;

    RDGraphNode* n;
    vect_each(n, nodes) {
        const RDEdgeVect* outgoing =
            rd_i_graph_get_outgoing_edges(self->graph, *n);

        usize n_outgoing = vect_length(outgoing);
        if(!n_outgoing) res++;
    }

    return res;
}

RDGraph* rd_function_get_graph(const RDFunction* self) { return self->graph; }

RDAddress rd_function_get_address(const RDFunction* self) {
    return self->address;
}

usize rd_function_get_n_instructions(const RDFunction* self) {
    return self->n_instructions;
}

bool rd_function_get_chunk(const RDFunction* self, RDGraphNode n,
                           RDFunctionChunk* chunk) {
    if(!self->graph) return false;

    const RDFunctionChunk* c = rd_i_function_get_chunk(self, n);
    if(!c) return false;
    if(chunk) *chunk = *c;
    return true;
}

const char* rd_function_generate_dot(const RDFunction* self) {
    if(self->graph) {
        return rd_graph_generate_dot(self->graph, _rd_function_dot_props,
                                     (RDFunction*)self);
    }

    return NULL;
}

u32 rd_function_get_hash(const RDFunction* self) {
    if(self->graph) {
        return rd_graph_get_hash(self->graph, _rd_function_dot_props,
                                 (RDFunction*)self);
    }

    return 0;
}

bool rd_function_contains_address(const RDFunction* self, RDAddress address) {
    if(!self->graph) return false;

    const RDNodeVect* nodes = rd_i_graph_get_nodes(self->graph);

    RDGraphNode* n;
    vect_each(n, nodes) {
        const RDFunctionChunk* chunk = rd_i_function_get_chunk(self, *n);
        if(chunk && (address >= chunk->start && address < chunk->end))
            return true;
    }

    return false;
}

void rd_i_rebuild_all_functions(RDContext* ctx) {
    LOG_INFO("generating functions...");
    const RDSegmentVect* segments = rd_i_db_get_segments(ctx);

    RDFunctionVect functions = {0};
    vect_reserve(&functions, vect_capacity(&ctx->functions));
    vect_reserve(&functions.chunks, vect_capacity(&ctx->functions.chunks));

    RDSegmentFull** it;
    vect_each(it, segments) {
        const RDSegmentFull* s = *it;
        if(!(s->base.perm & RD_SP_X)) continue;

        RDAddress address = s->base.start_address;

        for(usize i = 0; i < rd_flagsbuffer_get_length(s->flags);
            i++, address++) {
            if(!rd_flagsbuffer_has_func(s->flags, i)) continue;

            RDFunction* f = rd_i_function_create(ctx, address);
            _rd_function_rebuild_graph(f, &functions.chunks);
            vect_push(&functions, f);
        }
    }

    rd_i_functionchunk_sort(&functions.chunks);
    mem_swap(RDFunctionVect, &functions, &ctx->functions);
    rd_i_functionvect_destroy(&functions);
}

void rd_i_rebuild_function(RDFunction* f) {
    RDFunctionChunkVect* chunks = &f->context->functions.chunks;
    RDFunctionChunkVect* tmp_chunks = &f->context->chunk_buf;

    if(f->graph && !rd_graph_is_empty(f->graph))
        vect_del_if(chunks, &f->address, _rd_functionchunks_del_if_pred);

    assert(vect_is_empty(tmp_chunks));
    _rd_function_rebuild_graph(f, tmp_chunks);

    RDFunctionChunk** it;
    vect_each(it, tmp_chunks) {
        usize idx = vect_lower_bound(chunks, &(*it)->start,
                                     rd_i_functionchunk_kcmp_pred);
        vect_ins(chunks, idx, *it);
    }

    vect_clear(tmp_chunks);
}

void rd_i_functionvect_destroy(RDFunctionVect* self) {
    rd_i_functionchunk_destroy(&self->chunks);

    RDFunction** f;
    vect_each(f, self) { rd_i_function_destroy(*f); }
    vect_destroy(self);
}

int rd_i_functionchunk_kcmp_pred(const void* key, const void* item) {
    RDAddress address = *(const RDAddress*)key;
    const RDFunctionChunk* c = *(const RDFunctionChunk**)item;
    if(address < c->start) return -1;
    if(address >= c->end) return 1;
    return 0;
}

int rd_i_function_kcmp_pred(const void* key, const void* item) {
    RDAddress address = *(const RDAddress*)key;
    const RDFunction* f = *(const RDFunction**)item;
    if(address < f->address) return -1;
    if(address > f->address) return 1;
    return 0;
}
