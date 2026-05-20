#include "core/autorename.h"
#include "core/context.h"
#include "core/engine.h"
#include "core/stringfinder.h"
#include "listing/builder.h"
#include "support/containers.h"
#include "support/error.h"
#include "support/logging.h"

// clang-format off
typedef enum {
    RD_WS_INIT = 0,

    // 1st pass
    RD_WS_EMULATE1, RD_WS_KB1, RD_WS_ANALYZE1, 
    RD_WS_MERGEDATA1, 

    // 2nd pass
    RD_WS_EMULATE2, RD_WS_KB2, RD_WS_ANALYZE2, 
    RD_WS_MERGEDATA2, 

    // Finalize pass
    RD_WS_FINALIZE, RD_WS_DONE, 

    RD_WS_COUNT, 
} RDWorkerSteps;

static const char* const RD_STEP_NAMES[] = {
    "Init",
    "Emulate #1", "Knowledge Base #1", "Analyze #1", 
    "Merge Data #1",
    "Emulate #2", "Knowledge Base #2", "Analyze #2", 
    "Merge Data #2",
    "Finalize", "Done",
};
// clang-format on

typedef struct RDAddressQueue {
    RDAddress* data;
    usize length;
    usize capacity;
    usize head;
} RDAddressQueue;

static int _rd_worker_problem_cmp(const void* a, const void* b) {
    const RDProblem* pa = (const RDProblem*)a;
    const RDProblem* pb = (const RDProblem*)b;

    if(pa->from_address != pb->from_address)
        return pa->from_address < pb->from_address ? -1 : 1;
    if(pa->address != pb->address) return pa->address < pb->address ? -1 : 1;
    return 0;
}

static void _rd_worker_follow_pointers(RDContext* ctx) {
    LOG_INFO("following pointers");

    const RDSymbol* sym;
    vect_each(sym, &ctx->listing.symbols) {
        if(sym->kind != RD_SYMBOL_TYPE) continue;

        RDTypeFull t;
        if(!rd_i_get_type(ctx, sym->address, &t) || !rd_type_is_ptr(&t.base))
            continue;

        RDAddress dst;
        if(!rd_read_ptr(ctx, sym->address, &dst) || !rd_is_address(ctx, dst))
            continue;

        rd_add_xref(ctx, sym->address, dst, RD_DR_ADDRESS);
    }
}

static void _rd_worker_propagate_noret(RDContext* ctx, RDAddress funcaddr,
                                       RDXRefVect* xrefs_buf, RDAddressQueue* q,
                                       bool* changed) {
    const RDXRefVect* xrefs =
        rd_i_get_xrefs_to_ex(ctx, funcaddr, RD_CR_CALL, xrefs_buf);

    const RDXRef* r;
    vect_each(r, xrefs) {
        const RDSegmentFull* seg = rd_i_db_find_segment(ctx, r->address);
        assert(seg);

        usize idx = rd_i_address2index(seg, r->address);

        // already catched if NORET was available from the beginning
        // or other functions calls again this one
        if(rd_flagsbuffer_has_noret(seg->flags, idx)) continue;

        bool applied = rd_i_set_noret(ctx, seg, idx);

        if(applied) { // ...try to find upper layers...
            const RDFunction* f = rd_find_function(ctx, r->address);
            if(f) queue_push(q, f->address);
        }

        *changed |= applied;
    }
}

static void _rd_worker_apply_noret(RDContext* ctx) {
    RDXRefVect xrefs_buf = {0};
    RDAddressQueue q = {0};
    bool changed = false;

    RDTypeDef** it;
    vect_each(it, &ctx->types[RD_TKIND_FUNC]) {
        const RDTypeDef* tdef = *it;
        if(!tdef->func_.is_noret) continue;

        RDAddress address;
        if(!rd_get_address(ctx, tdef->name, &address)) continue;

        _rd_worker_propagate_noret(ctx, address, &xrefs_buf, &q, &changed);
    }

    // look for who references them and propagate NORET
    // (needed if KB is not available & processor plugins stamps NORET too)
    RDFunction** f;
    vect_each(f, &ctx->functions) {
        if(!rd_i_function_has_noret(*f)) continue;
        queue_push(&q, (*f)->address);
    }

    // queue propagates NORET to callers and callers of callers...
    while(!queue_is_empty(&q)) {
        RDAddress funcaddr;
        queue_pop(&q, &funcaddr);
        _rd_worker_propagate_noret(ctx, funcaddr, &xrefs_buf, &q, &changed);
    }

    if(changed) rd_i_rebuild_all_functions(ctx);

    queue_destroy(&q);
    vect_destroy(&xrefs_buf);
}

static void _rd_worker_step_init(RDContext* ctx, RDWorkerStatus* status) {
    rd_i_listing_build(ctx); // Show pre-analysis listing
    if(status) status->is_listing_changed = true;
    ctx->engine.step++;
}

static void _rd_worker_step_emulate(RDContext* ctx, RDWorkerStatus* status) {
    if(!ctx->engine.emulate_start) ctx->engine.emulate_start = clock();

    if(rd_i_engine_has_pending_code(ctx)) {
        rd_i_engine_tick(ctx);
        if(status) optional_set(&status->address, ctx->engine.current.address);
    }
    else {
        double elapsed =
            (double)(clock() - ctx->engine.emulate_start) / CLOCKS_PER_SEC;
        LOG_INFO("completed in %.2fs", elapsed);
        ctx->engine.emulate_start = 0;
        ctx->engine.step++;
    }
}

static void _rd_worker_step_kb(RDContext* ctx) {
    // if(!vect_is_empty(&ctx->kb->noret_names)) _rd_worker_apply_noret(ctx);

    ctx->engine.step++;
}

static void _rd_worker_step_analyze(RDContext* ctx) {
    RDAnalyzerItem** it;
    vect_each(it, &ctx->analyzerplugins) {
        RDAnalyzerItem* ai = *it;

        if(!ai->is_selected ||
           ((ai->plugin->flags & RD_AF_RUNONCE) && ai->n_runs > 0))
            continue;

        ai->n_runs++;
        rd_reader_seek(ctx->input_reader, 0);
        ai->plugin->execute(ctx);
    }

    rd_reader_seek(ctx->input_reader, 0);

    if(rd_i_engine_has_pending_code(ctx)) {
        if(ctx->engine.step == RD_WS_ANALYZE1)
            ctx->engine.step = RD_WS_EMULATE1;
        else if(ctx->engine.step == RD_WS_ANALYZE2)
            ctx->engine.step = RD_WS_EMULATE2;
        else
            unreachable();
    }
    else
        ctx->engine.step++;
}

static void _rd_worker_step_mergedata(RDContext* ctx) {
    rd_i_find_strings(ctx);
    ctx->engine.step++;
}

static void _rd_worker_step_finalize(RDContext* ctx, RDWorkerStatus* status) {
    rd_i_rebuild_all_functions(ctx);
    _rd_worker_follow_pointers(ctx);
    rd_i_autorename(ctx);
    _rd_worker_apply_noret(ctx);
    rd_i_listing_build(ctx);
    rd_fire_hook(ctx, "redasm.finalize");
    vect_sort(&ctx->problems, _rd_worker_problem_cmp);

    if(status) status->is_listing_changed = true;
    ctx->engine.step++;

    rd_i_db_flush(ctx);

    // post-analysis summary
    LOG_INFO("terminated with functions: %zu, symbols: %zu, problems: %zu",
             vect_length(&ctx->functions), vect_length(&ctx->listing.symbols),
             vect_length(&ctx->problems));
}

bool rd_is_busy(const RDContext* self) {
    return self->engine.step < RD_WS_DONE || rd_i_engine_has_pending_code(self);
}

bool rd_step(RDContext* self, RDWorkerStatus* status) {
    assert(self->engine.step < RD_WS_COUNT);
    bool is_busy = self->engine.step < RD_WS_DONE;

    if(status) {
        status->is_busy = is_busy;
        status->step = RD_STEP_NAMES[self->engine.step];
        status->segment = (const RDSegment*)self->engine.segment;
        status->is_listing_changed = false;
        status->pending_calls = queue_length(&self->engine.qcall);
        status->pending_jumps = queue_length(&self->engine.qjump);
        optional_unset(&status->address);
    }

    if(is_busy) {
        switch(self->engine.step) {
            case RD_WS_INIT: _rd_worker_step_init(self, status); break;

            case RD_WS_EMULATE1:
            case RD_WS_EMULATE2: _rd_worker_step_emulate(self, status); break;

            case RD_WS_KB1:
            case RD_WS_KB2: _rd_worker_step_kb(self); break;

            case RD_WS_ANALYZE1:
            case RD_WS_ANALYZE2: _rd_worker_step_analyze(self); break;

            case RD_WS_MERGEDATA1:
            case RD_WS_MERGEDATA2: _rd_worker_step_mergedata(self); break;

            case RD_WS_FINALIZE: _rd_worker_step_finalize(self, status); break;
            default: unreachable();
        }
    }

    return is_busy;
}

void rd_disassemble(RDContext* ctx) {
    while(rd_step(ctx, NULL))
        ;
}

const char* rd_str_intern(RDContext* self, const char* s) {
    return rd_i_strpool_intern(&self->strings, s);
}
