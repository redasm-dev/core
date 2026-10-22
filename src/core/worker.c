#include "worker.h"
#include "core/argdiscover.h"
#include "core/autorename.h"
#include "core/context.h"
#include "core/engine.h"
#include "core/stringfinder.h"
#include "core/undefine.h"
#include "function/function.h"
#include "io/flagsbuffer.h"
#include "plugins/analyzer.h"
#include "support/containers.h"
#include "support/error.h"
#include <redasm/support/logging.h>

#if !defined(NDEBUG)
static void _rd_worker_check_chunk_pair(const RDContext* ctx, const RDChunk* p,
                                        const RDChunk* c) {
    if(p->end <= c->start) return;

    RD_LOG_FAIL("chunk overlap: func %llx [%llx,%llx) vs func %llx [%llx,%llx)",
                (unsigned long long)rd_i_abs(ctx, p->entry),
                (unsigned long long)rd_i_abs(ctx, p->start),
                (unsigned long long)rd_i_abs(ctx, p->end),
                (unsigned long long)rd_i_abs(ctx, c->entry),
                (unsigned long long)rd_i_abs(ctx, c->start),
                (unsigned long long)rd_i_abs(ctx, c->end));
}
#endif

static void _rd_worker_next_or_emulate(RDContext* ctx) {
    if(rd_i_engine_has_pending_code(ctx))
        ctx->engine.step = RD_WS_EMULATE;
    else
        ctx->engine.step++;
}

static void _rd_worker_rebuild_chunks(RDContext* ctx) {
    RD_LOG_INFO("generating chunks");
    rd_i_chunks_build_all(ctx);
}

static void _rd_worker_follow_pointers(RDContext* ctx) {
    RD_LOG_INFO("following pointers");

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

        if(tdef->kind != RD_TKIND_FUNC || (tdef->flags & RD_TFLAGS_BUILTIN) ||
           (tdef->flags & RD_TFLAGS_PROTOTYPE))
            continue;

        RDAddress address;
        if(!rd_get_address(ctx, tdef->name, &address)) continue;

        const RDSegment* seg = rd_i_db_find_segment(ctx, address);
        if(!seg) continue;

        usize idx = rd_i_address2index(seg, address);

        if(rd_flagsbuffer_has_func(seg->flags, idx)) {
            const RDTypeDef* func_tdef = rd_function_get_type_def(ctx, address);

            if(!func_tdef || func_tdef == tdef ||
               (func_tdef->flags & RD_TFLAGS_BUILTIN))
                continue;

            rd_i_function_set_type_def(ctx, address, tdef);
        }
        else if(rd_flagsbuffer_has_type(seg->flags, idx)) {
            RDType t;
            if(!rd_i_db_get_type(ctx, address, &t) || t.def == tdef) continue;
            if(t.count > 0 || t.def->kind != RD_TKIND_FUNC) continue;
            if(!(t.def->flags & RD_TFLAGS_BUILTIN)) continue;

            rd_i_set_type(ctx, address, tdef->name, 0, t.mod,
                          RD_CONFIDENCE_LIBRARY);
        }
    }
}

static void _rd_worker_apply_noret(RDContext* ctx) {
    RDXRefVect xrefs = {0};
    RDAddressVect v = {0};
    RDAddressVect functions = {0};

    rd_i_db_get_all_functions(ctx, &functions); // snapshot

    const RDAddress* fa;
    vect_each(fa, &functions) {
        if(rd_function_is_noret(ctx, *fa)) vect_push(&v, *fa);
    }

    while(!vect_is_empty(&v)) {
        RDAddress address = vect_pop_last(&v);
        rd_i_get_xrefs_to_ex(ctx, address, RD_XR_NONE, &xrefs);

        const RDXRef* r;
        vect_each(r, &xrefs) {
            if(r->type != RD_CR_JUMP && r->type != RD_CR_CALL) continue;

            // stamp FL_NORET on the call site
            if(!rd_i_set_noret(ctx, r->address)) continue;

            // containing function: copy the entry out, the rebuild below
            // reallocates ctx->chunks
            const RDChunk* c = rd_i_chunks_find(ctx, r->address);
            if(!c) continue;

            RDAddress entry = rd_i_abs(ctx, c->entry);
            rd_i_chunks_rebuild_at(ctx, entry);

            // if all exit blocks are now NORET, propagate further
            if(rd_function_is_noret(ctx, entry)) vect_push(&v, entry);
        }
    }

    vect_destroy(&v);
    vect_destroy(&functions);
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
    RD_LOG_INFO("deduping names");

    // duplicate vector because rd_i_set_name mutates ctx->pending_renames
    RDPendingRenameVect pending = {0};
    vect_dup(&pending, &ctx->pending_renames);

    const RDPendingRename* p;
    vect_each(p, &pending)
        rd_i_set_name(ctx, p->address, p->name.value, p->name.confidence);

    vect_destroy(&pending);
    vect_clear(&ctx->pending_renames);
}

static void _rd_worker_reconcile_data(RDContext* ctx, const RDSegment* seg,
                                      usize start, usize end) {
    RDAddress curr = rd_segment_get_start(seg) + start;

    for(usize i = start; i < end; i++, curr++) {
        if(!rd_i_flagsbuffer_has_xref_out(seg->flags, i)) continue;

        const RDXRefVect* refs =
            rd_i_get_xrefs_from_ex(ctx, curr, RD_XR_NONE, &ctx->und_xrefs);

        const RDXRef* r;
        vect_each(r, refs)
            rd_i_del_xref(ctx, curr, r->address, RD_CONFIDENCE_MAX);
    }
}

static void _rd_worker_step_init(RDContext* ctx, RDWorkerStatus* status) {
    if(status) status->reconcile = false;
    ctx->engine.step++;
}

static void _rd_worker_step_reconcile(RDContext* ctx, RDWorkerStatus* status) {
    if(status) status->reconcile = true;

    RDEngineItem* item;
    queue_each(item, &ctx->engine.qdirty) {
        const RDSegment* seg = rd_i_db_find_segment(ctx, item->address);
        if(!seg) continue;

        usize start_idx = rd_i_address2index(seg, item->address);
        usize end_idx = start_idx + item->n;
        rd_i_expand_range(ctx, seg, &start_idx, &end_idx);

        if(item->kind == RD_EI_CODE) goto keep;

        if(rd_flagsbuffer_has_code(seg->flags, start_idx)) {
            RDAddress start, end;
            bool ok = rd_i_undefine_range(ctx, item->address, item->n,
                                          RD_CONFIDENCE_MAX, RD_UNDEFINE_CLEAR,
                                          &start, &end);
            assert(ok && "reconcile: a forced undefine cannot be rejected");
            RD_UNUSED(ok);

            start_idx = start - rd_segment_get_start(seg);
            end_idx = end - rd_segment_get_start(seg);
            goto keep;
        }

        if(rd_flagsbuffer_has_data(seg->flags, start_idx))
            _rd_worker_reconcile_data(ctx, seg, start_idx, end_idx);

        // data or unknown: nothing left for EMULATE
        hmap_destroy(&item->registers);
        item->kind = RD_EI_NONE;
        continue;

    keep:
        // decode from the ITEM HEAD
        // a mid-item patch must not decode from the patched byte
        item->address = rd_segment_get_start(seg) + start_idx;
        item->from = item->address;
        item->n = end_idx - start_idx;
    }

    ctx->engine.step = RD_WS_EMULATE;
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
        RD_LOG_INFO("completed in %.2fs", elapsed);
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

    _rd_worker_next_or_emulate(ctx);
}

static void _rd_worker_step_symbols(RDContext* ctx) {
    rd_fire_hook(ctx, "redasm.symbols");

    _rd_worker_resolve_ordinals(ctx);
    _rd_worker_dedup_names(ctx);
    rd_i_autorename(ctx);
    _rd_worker_apply_function_types(ctx);
    rd_i_discover_args(ctx);

    _rd_worker_next_or_emulate(ctx);
}

static void _rd_worker_step_finalize(RDContext* ctx) {
    rd_i_find_strings(ctx);
    _rd_worker_rebuild_chunks(ctx);
    _rd_worker_follow_pointers(ctx);
    _rd_worker_apply_noret(ctx);

#if !defined(NDEBUG)
    for(usize i = 1; i < vect_length(&ctx->chunks); i++) {
        _rd_worker_check_chunk_pair(ctx, vect_at(&ctx->chunks, i - 1),
                                    vect_at(&ctx->chunks, i));
    }
#endif

    rd_fire_hook(ctx, "redasm.finalized");

    ctx->engine.step++;

    RD_LOG_INFO("terminated with functions: %zu, problems: %zu",
                rd_i_db_get_function_count(ctx),
                rd_i_db_get_problem_count(ctx));
}

const char* rd_i_worker_get_step_name(RDWorkerSteps s) {
    switch(s) {
        RD_WORKER_STEPS(RD_WORKER_STEP_CASE_X)
        default: unreachable();
    }

    return NULL;
}

bool rd_step(RDContext* self, RDWorkerStatus* status) {
    if(!self) return false;

    assert(self->engine.step < RD_WS_COUNT);
    bool is_busy = self->engine.step < RD_WS_DONE;

    if(status) {
        status->is_busy = is_busy;
        status->step = rd_i_worker_get_step_name(self->engine.step);
        status->segment = self->engine.segment;
        status->pending_calls = queue_length(&self->engine.qcall);
        status->pending_jumps = queue_length(&self->engine.qjump);
        optional_unset(&status->address);
    }

    if(is_busy) {
        // clang-format off
        switch(self->engine.step) {
            case RD_WS_INIT: _rd_worker_step_init(self, status); break;
            case RD_WS_RECONCILE: _rd_worker_step_reconcile(self, status); break;
            case RD_WS_EMULATE: _rd_worker_step_emulate(self, status); break;
            case RD_WS_ANALYZE: _rd_worker_step_analyze(self); break;
            case RD_WS_SYMBOLS: _rd_worker_step_symbols(self); break;
            case RD_WS_FINALIZE: _rd_worker_step_finalize(self); break;
            default: unreachable();
        }
        // clang-format on
    }

    return is_busy;
}

bool rd_reanalyze(RDContext* self) { return rd_i_engine_mark_dirty(self); }

void rd_disassemble(RDContext* ctx) {
    while(rd_step(ctx, NULL))
        ;
}
