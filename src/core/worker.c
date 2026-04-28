#include "core/context.h"
#include "core/engine.h"
#include "core/stringfinder.h"
#include "io/flagsbuffer.h"
#include "listing/builder.h"
#include "support/containers.h"
#include "support/error.h"
#include "support/pattern.h"

// clang-format off
typedef enum {
    RD_WS_INIT = 0,

    // 1st pass
    RD_WS_EMULATE1, RD_WS_ANALYZE1, 
    RD_WS_SIGNATURE1, RD_WS_MERGEDATA1, 

    // 2nd pass
    RD_WS_MERGECODE, 
    RD_WS_EMULATE2, RD_WS_ANALYZE2, 
    RD_WS_SIGNATURE2, RD_WS_MERGEDATA2, 

    // Finalize pass
    RD_WS_FINALIZE, RD_WS_DONE, 

    RD_WS_COUNT, 
} RDWorkerSteps;

static const char* const RD_STEP_NAMES[] = {
    "Init",
    "Emulate #1", "Analyze #1", 
    "Signature #1", "Merge Data #1",
    "Merge Code",
    "Emulate #2", "Analyze #2", 
    "Signature #2", "Merge Data #2",
    "Finalize", "Done",
};

// clang-format on

static int _rd_worker_problem_cmp(const void* a, const void* b) {
    const RDProblem* pa = (const RDProblem*)a;
    const RDProblem* pb = (const RDProblem*)b;

    if(pa->from_address != pb->from_address)
        return pa->from_address < pb->from_address ? -1 : 1;
    if(pa->address != pb->address) return pa->address < pb->address ? -1 : 1;
    return 0;
}

static void _rd_worker_promote_refs(RDContext* ctx) {
    RDSegmentFull** it;

    vect_each(it, &ctx->segments) {
        const RDSegmentFull* seg = *it;
        if(!(seg->base.perm & RD_SP_X)) continue;

        RDFlagsBuffer* flags = seg->flags;

        for(usize i = 0; i < flags->base.length; i++) {
            if(!rd_i_flagsbuffer_has_xref_in(seg->flags, i)) continue;
            if(rd_flagsbuffer_has_code(seg->flags, i)) continue;
            if(rd_flagsbuffer_has_tail(seg->flags, i)) continue;

            RDAddress addr = rd_i_index2address(seg, i);
            const RDXRefVect* refs =
                rd_i_get_xrefs_to_type(ctx, addr, RD_DR_ADDRESS);

            if(refs)
                rd_i_engine_enqueue_call(ctx, addr, NULL, RD_CONFIDENCE_AUTO);
        }
    }
}

static void _rd_worker_scan_prologues(RDContext* ctx) {
    if(!ctx->processorplugin->get_prologues) return;

    const char** prologues =
        ctx->processorplugin->get_prologues(ctx->processor, ctx);
    if(!prologues || !(*prologues)) return;

    RDPattern pattern = {0};

    for(const char** p = prologues; *p; p++) {
        if(!rd_pattern_compile(&pattern, *p)) continue;

        RDSegmentFull** it;
        vect_each(it, &ctx->segments) {
            if(!((*it)->base.perm & RD_SP_X)) continue;

            const RDFlagsBuffer* flags = (*it)->flags;

            for(usize i = 0; i < flags->base.length;) {
                if(rd_flagsbuffer_has_tail(flags, i)) {
                    i++;
                    continue;
                }

                bool is_candidate = rd_flagsbuffer_has_unknown(flags, i) ||
                                    (rd_flagsbuffer_has_code(flags, i) &&
                                     !rd_flagsbuffer_has_func(flags, i));

                if(is_candidate) {
                    if(rd_pattern_match(&pattern, flags, i)) {
                        RDAddress addr = (*it)->base.start_address + i;
                        rd_auto_function(ctx, addr, NULL);
                    }

                    i++;
                }
                else {
                    assert(!rd_flagsbuffer_has_tail(flags, i));
                    i += rd_i_flagsbuffer_get_range_length(flags, i);
                }
            }
        }
    }

    rd_pattern_destroy(&pattern);
}

static void _rd_worker_step_init(RDContext* ctx) {
    rd_i_listing_build(ctx); // Show pre-analysis listing
    ctx->engine.status.is_listing_changed = true;
    ctx->engine.step++;
}

static void _rd_worker_step_emulate(RDContext* ctx) {
    if(rd_i_engine_has_pending_code(ctx)) {
        rd_i_engine_tick(ctx);
        optional_set(&ctx->engine.status.address, ctx->engine.current.address);
    }
    else
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
        ai->plugin->execute(ctx);
    }

    if(ctx->engine.step == RD_WS_ANALYZE2) _rd_worker_scan_prologues(ctx);

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

static void _rd_worker_step_mergecode(RDContext* ctx) {
    _rd_worker_promote_refs(ctx);
    ctx->engine.step++;
}

static void _rd_worker_step_mergedata(RDContext* ctx) {
    rd_i_find_strings(ctx);
    ctx->engine.step++;
}

static void _rd_worker_step_signature(RDContext* ctx) { ctx->engine.step++; }

static void _rd_worker_step_finalize(RDContext* ctx) {
    rd_i_listing_build(ctx);
    vect_sort(&ctx->problems, _rd_worker_problem_cmp);
    ctx->engine.status.is_listing_changed = true;
    ctx->engine.step++;
}

bool rd_step(RDContext* ctx, const RDWorkerStatus** status) {
    assert(ctx->engine.step < RD_WS_COUNT);
    ctx->engine.status.is_busy = ctx->engine.step < RD_WS_DONE;
    ctx->engine.status.step = RD_STEP_NAMES[ctx->engine.step];
    ctx->engine.status.segment = (const RDSegment*)ctx->engine.segment;
    ctx->engine.status.is_listing_changed = false;
    ctx->engine.status.pending_calls = queue_length(&ctx->engine.qcall);
    ctx->engine.status.pending_jumps = queue_length(&ctx->engine.qjump);
    optional_unset(&ctx->engine.status.address);

    if(ctx->engine.status.is_busy) {
        switch(ctx->engine.step) {
            case RD_WS_INIT: _rd_worker_step_init(ctx); break;

            case RD_WS_EMULATE1:
            case RD_WS_EMULATE2: _rd_worker_step_emulate(ctx); break;

            case RD_WS_ANALYZE1:
            case RD_WS_ANALYZE2: _rd_worker_step_analyze(ctx); break;

            case RD_WS_MERGECODE: _rd_worker_step_mergecode(ctx); break;

            case RD_WS_MERGEDATA1:
            case RD_WS_MERGEDATA2: _rd_worker_step_mergedata(ctx); break;

            case RD_WS_SIGNATURE1:
            case RD_WS_SIGNATURE2: _rd_worker_step_signature(ctx); break;

            case RD_WS_FINALIZE: _rd_worker_step_finalize(ctx); break;
            default: unreachable();
        }
    }

    if(status) *status = &ctx->engine.status;
    return ctx->engine.status.is_busy;
}

void rd_disassemble(RDContext* ctx) {
    while(rd_step(ctx, NULL))
        ;
}
