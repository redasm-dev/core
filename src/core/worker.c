#include "worker.h"
#include "core/autorename.h"
#include "core/context.h"
#include "core/engine.h"
#include "core/stringfinder.h"
#include "listing/builder.h"
#include "plugins/analyzer.h"
#include "support/containers.h"
#include "support/error.h"
#include "support/logging.h"

// clang-format off
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

static int _rd_worker_problem_cmp(const void* a, const void* b) {
    const RDProblem* pa = (const RDProblem*)a;
    const RDProblem* pb = (const RDProblem*)b;

    if(pa->from_address != pb->from_address)
        return pa->from_address < pb->from_address ? -1 : 1;
    if(pa->address != pb->address) return pa->address < pb->address ? -1 : 1;
    return 0;
}

static void _rd_worker_rebuild_functions(RDContext* ctx) {
    LOG_INFO("generating functions");
    const RDSegmentFullVect* segments = rd_i_db_get_segments(ctx);

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
            rd_i_function_rebuild_graph(f, &functions.chunks);
            vect_push(&functions, f);
        }
    }

    rd_i_functionchunk_sort(&functions.chunks);
    mem_swap(RDFunctionVect, &functions, &ctx->functions);
    rd_i_functionvect_destroy(&functions);
}

static void _rd_worker_follow_pointers(RDContext* ctx) {
    LOG_INFO("following pointers");

    RDAddressVect addresses = {0};
    RDTypeVect types = {0};
    rd_i_db_get_all_types(ctx, &addresses, &types);

    for(usize i = 0; i < vect_length(&addresses); i++) {
        const RDType* t = vect_at(&types, i);
        if(!rd_type_is_ptr(t)) continue;

        RDAddress address = *vect_at(&addresses, i);

        RDAddress dst;
        if(!rd_read_ptr(ctx, address, &dst) || !rd_is_address(ctx, dst))
            continue;

        rd_add_xref(ctx, address, dst, RD_DR_ADDRESS);
    }

    vect_destroy(&types);
    vect_destroy(&addresses);
}

static void _rd_worker_apply_function_types(RDContext* ctx) {
    RDTypeDef** it;
    vect_each(it, &ctx->typedefs) {
        const RDTypeDef* tdef = *it;
        if(tdef->kind != RD_TKIND_FUNC) continue;

        RDAddress address;
        if(!rd_get_address(ctx, tdef->name, &address)) continue;

        RDFunction* f = rd_i_find_function(ctx, address);
        if(f) rd_i_function_set_type_def(f, tdef);
    }
}

static void _rd_worker_apply_noret(RDContext* ctx) {
    RDXRefVect xrefs = {0};
    RDAddressVect v = {0};

    RDFunction** func_it;
    vect_each(func_it, &ctx->functions) {
        if(!rd_function_is_noret(*func_it)) continue;
        vect_push(&v, (*func_it)->address);
    }

    while(!vect_is_empty(&v)) {
        RDAddress address = vect_pop_last(&v);
        rd_i_get_xrefs_to_ex(ctx, address, RD_XR_NONE, &xrefs);

        const RDXRef* r;
        vect_each(r, &xrefs) {
            if(r->type != RD_CR_JUMP && r->type != RD_CR_CALL) continue;

            // stamp FL_NORET on the call site
            if(!rd_i_set_noret(ctx, r->address)) continue;

            // find containing function, rebuild its graph
            RDFunction* f = rd_i_find_function(ctx, r->address);
            if(!f) continue;

            rd_i_function_rebuild(f);

            // if all exit blocks are now NORET, propagate further
            if(rd_function_is_noret(f)) vect_push(&v, f->address);
        }
    }

    vect_destroy(&v);
    vect_destroy(&xrefs);
}

static void _rd_worker_resolve_ordinals(RDContext* ctx) {
    RDExternalVect v = {0};
    rd_i_db_get_all_externals(ctx, RD_EXT_NONE, &v);

    RDExternal* ext;
    vect_each(ext, &v) {
        if(!ext->ordinal.has_value) continue;

        const char* name =
            rd_i_kb_find_ordinal_name(ctx, ext->module, ext->ordinal.value);

        if(name) rd_i_set_name(ctx, ext->address, name, RD_CONFIDENCE_LIBRARY);
    }

    vect_destroy(&v);
}

// check if duplicate names are now free
static void _rd_worker_dedup_names(RDContext* ctx) {
    LOG_INFO("deduping names");

    // duplicate vector because rd_i_set_name mutates ctx->pending_renames
    RDPendingRenameVect pending = {0};
    vect_dup(&pending, &ctx->pending_renames);

    const RDPendingRename* p;
    vect_each(p, &pending)
        rd_i_set_name(ctx, p->address, p->name.value, p->name.confidence);

    vect_destroy(&pending);
    vect_clear(&ctx->pending_renames);
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
    _rd_worker_rebuild_functions(ctx);
    _rd_worker_follow_pointers(ctx);
    _rd_worker_resolve_ordinals(ctx);
    _rd_worker_dedup_names(ctx);
    rd_i_autorename(ctx);
    _rd_worker_apply_function_types(ctx);
    _rd_worker_apply_noret(ctx);
    rd_i_listing_build(ctx);
    rd_fire_hook(ctx, "redasm.finalized");
    vect_sort(&ctx->problems, _rd_worker_problem_cmp);

    if(status) status->is_listing_changed = true;
    ctx->engine.step++;

    rd_i_db_save(ctx);

    // post-analysis summary
    LOG_INFO("terminated with functions: %zu, symbols: %zu, problems: %zu",
             vect_length(&ctx->functions), vect_length(&ctx->listing.symbols),
             vect_length(&ctx->problems));
}

bool rd_step(RDContext* self, RDWorkerStatus* status) {
    if(!self) return false;

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
