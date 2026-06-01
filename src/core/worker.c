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
    RD_WS_EMULATE1, RD_WS_ANALYZE1, 
    RD_WS_MERGEDATA1, 

    // 2nd pass
    RD_WS_EMULATE2, RD_WS_ANALYZE2, 
    RD_WS_MERGEDATA2, 

    // Finalize pass
    RD_WS_FINALIZE, RD_WS_DONE, 

    RD_WS_COUNT, 
} RDWorkerSteps;

static const char* const RD_STEP_NAMES[] = {
    "Init",
    "Emulate #1", "Analyze #1", 
    "Merge Data #1",
    "Emulate #2","Analyze #2", 
    "Merge Data #2",
    "Finalize", "Done",
};
// clang-format on

static_assert(rd_count_of(RD_STEP_NAMES) == RD_WS_COUNT, "step names mismatch");

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

static void _rd_worker_apply_noret(RDContext* ctx) {
    RDXRefVect xrefs = {0};
    RDAddressVect v = {0};

    RDFunction** f;
    vect_each(f, &ctx->functions) {
        if(!rd_i_function_is_noret(*f)) continue;
        vect_push(&v, (*f)->address);
    }

    // integrate with KB, if available
    RDTypeDef** it;
    vect_each(it, &ctx->types[RD_TKIND_FUNC]) {
        const RDTypeDef* tdef = *it;
        if(!tdef->func_.is_noret) continue;

        RDAddress address;
        if(!rd_get_address(ctx, tdef->name, &address)) continue;

        usize idx = vect_lower_bound(&v, &address, rd_i_address_kcmp_pred);
        if(idx < vect_length(&v) && *vect_at(&v, idx) == address) continue;

        vect_ins(&v, idx, address);
    }

    while(!vect_is_empty(&v)) {
        RDAddress address = vect_pop_last(&v);
        rd_i_get_xrefs_to_ex(ctx, address, RD_CR_CALL, &xrefs);

        const RDXRef* r;
        vect_each(r, &xrefs) {
            // stamp FL_NORET on the call site
            if(!rd_i_set_noret(ctx, r->address)) continue;

            // find containing function, rebuild its graph
            RDFunction* f = rd_i_find_function(ctx, r->address);
            if(!f) continue;

            rd_i_rebuild_function(f);

            // if all exit blocks are now NORET, propagate further
            if(rd_i_function_is_noret(f)) vect_push(&v, f->address);
        }
    }

    vect_destroy(&v);
    vect_destroy(&xrefs);
}

static void _rd_worker_resolve_ordinals(RDContext* ctx) {
    const RDAddress* it;
    vect_each(it, &ctx->imported) {
        RDImported imp;
        if(!rd_get_imported(ctx, *it, &imp) || !imp.ordinal.has_value) continue;

        const char* name =
            rd_i_kb_find_ordinal_name(ctx, imp.module, imp.ordinal.value);
        if(name) rd_set_imported(ctx, *it, imp.module, name);
    }
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
    _rd_worker_resolve_ordinals(ctx);
    rd_i_autorename(ctx);
    _rd_worker_apply_noret(ctx);
    rd_i_listing_build(ctx);
    rd_fire_hook(ctx, "redasm.finalized");
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

void rd_set_scan_char16(RDContext* self, bool b) { self->scan_char16 = b; }

const char* rd_get_loader_name(const RDContext* self) {
    return self->loader_name;
}

const char* rd_str_intern(RDContext* self, const char* s) {
    return rd_i_strpool_intern(&self->strings, s);
}
