#include "engine.h"
#include "core/context.h"
#include "io/flagsbuffer.h"
#include "support/containers.h"

static bool _rd_engine_accept_address(const RDContext* ctx, RDAddress address,
                                      RDWorkerQueue* q) {
    if(!queue_is_empty(q) && queue_peek_back(q).address == address)
        return false;

    const RDSegmentFull* seg = ctx->worker.segment;

    if(!seg || address < seg->base.start_address ||
       address >= seg->base.end_address)
        seg = rd_i_find_segment(ctx, address);

    return seg && (seg->base.perm & RD_SP_X);
}

static void _rd_engine_execute_delay_slots(RDContext* ctx,
                                           const RDInstruction* instr) {
    ctx->worker.dslot_info.instr = *instr;
    ctx->worker.dslot_info.n = 1;
    RDAddress address = instr->address + instr->length;

    while(ctx->worker.dslot_info.n <=
          ctx->worker.dslot_info.instr.delay_slots) {
        rd_flow(ctx, address);
        u32 len = rd_i_engine_tick(ctx);

        if(!len) {
            rd_i_add_problem(ctx, instr->address, address,
                             "cannot decode delay slot #%u",
                             ctx->worker.dslot_info.n);
            break;
        }

        address += len;
        ctx->worker.dslot_info.n++;
    }

    ctx->worker.dslot_info = (RDDelaySlotInfo){0};
}

bool rd_i_engine_decode(RDContext* ctx, RDAddress address,
                        RDInstruction* instr) {
    assert(ctx->processorplugin->decode && "missing instruction decoder");
    instr->address = address;
    ctx->processorplugin->decode(ctx, instr, ctx->processor);
    return instr->length > 0;
}

bool rd_i_engine_enqueue_jump(RDContext* ctx, RDAddress addr) {
    if(_rd_engine_accept_address(ctx, addr, &ctx->worker.qjump)) {
        queue_push(&ctx->worker.qjump, (RDWorkerItem){
                                           .kind = RD_WI_JUMP,
                                           .address = addr,
                                       });
        return true;
    }

    return false;
}

bool rd_i_engine_enqueue_call(RDContext* ctx, RDAddress addr, const char* name,
                              RDConfidence c) {
    if(_rd_engine_accept_address(ctx, addr, &ctx->worker.qcall)) {
        queue_push(&ctx->worker.qcall,
                   (RDWorkerItem){
                       .kind = RD_WI_CALL,
                       .confidence = c,
                       .address = addr,
                       .name = name ? rd_strdup(name) : NULL,
                   });
        return true;
    }

    return false;
}

bool rd_i_engine_has_pending_code(const RDContext* ctx) {
    return ctx->processorplugin->emulate &&
           (ctx->worker.flow.kind == RD_WI_FLOW ||
            !queue_is_empty(&ctx->worker.qjump) ||
            !queue_is_empty(&ctx->worker.qcall));
}

u16 rd_i_engine_tick(RDContext* ctx) {
    if(ctx->worker.flow.kind == RD_WI_FLOW) {
        ctx->worker.current = ctx->worker.flow;
        ctx->worker.flow.kind = RD_WI_NONE;
    }
    else if(!queue_is_empty(&ctx->worker.qcall)) {
        queue_pop(&ctx->worker.qcall, &ctx->worker.current);
    }
    else if(!queue_is_empty(&ctx->worker.qjump)) {
        queue_pop(&ctx->worker.qjump, &ctx->worker.current);
    }
    else
        return 0;

    ctx->worker.segment = rd_i_find_segment(ctx, ctx->worker.current.address);
    assert(ctx->worker.segment && "program counter without segment");
    if(!(ctx->worker.segment->base.perm & RD_SP_X)) goto done;

    usize idx =
        rd_i_address2index(ctx->worker.segment, ctx->worker.current.address);

    RDInstruction instr = {
        .delay_slots = ctx->worker.dslot_info.n ? RD_IS_DSLOT : 0,
    };

    if(rd_flagsbuffer_has_code(ctx->worker.segment->flags, idx)) {
        instr.length =
            rd_i_flagsbuffer_get_range_length(ctx->worker.segment->flags, idx);

        if(ctx->worker.current.kind == RD_WI_JUMP) {
            // loops case: destination gets decoded before source
            rd_i_flagsbuffer_set_jmpdst(ctx->worker.segment->flags, idx);
        }
        else if(ctx->worker.current.kind == RD_WI_CALL) {
            // reclassified locations as function
            rd_i_flagsbuffer_set_func(ctx->worker.segment->flags, idx);
        }

        if(ctx->worker.current.name) {
            rd_i_set_name(ctx, ctx->worker.current.address,
                          ctx->worker.current.name,
                          ctx->worker.current.confidence);
        }

        goto done;
    }

    if(!rd_i_engine_decode(ctx, ctx->worker.current.address, &instr)) goto done;

    if(instr.length) {
        if(!rd_i_flagsbuffer_has_unknown_n(ctx->worker.segment->flags, idx,
                                           instr.length)) {
            rd_i_add_problem(ctx, ctx->worker.current.address,
                             ctx->worker.current.address,
                             "instruction overlaps existing item (length: %u)",
                             instr.length);
            instr.length = 0;
            goto done;
        }

        if(!rd_i_flagsbuffer_set_code(ctx->worker.segment->flags, idx,
                                      instr.length)) {
            rd_i_add_problem(
                ctx, ctx->worker.current.address, ctx->worker.current.address,
                "failed to classify as code (length: %u)", instr.length);
            instr.length = 0;
            goto done;
        }

        const RDProcessorPlugin* plugin = ctx->processorplugin;
        assert(plugin->emulate);
        plugin->emulate(ctx, &instr, ctx->processor);

        if(instr.flow == RD_IF_JUMP || instr.flow == RD_IF_JUMP_COND)
            rd_i_flagsbuffer_set_jump(ctx->worker.segment->flags, idx);
        if(instr.flow == RD_IF_CALL || instr.flow == RD_IF_CALL_COND)
            rd_i_flagsbuffer_set_call(ctx->worker.segment->flags, idx);

        if(instr.flow == RD_IF_JUMP_COND || instr.flow == RD_IF_CALL_COND)
            rd_i_flagsbuffer_set_cond(ctx->worker.segment->flags, idx);

        if(ctx->worker.current.kind == RD_WI_FLOW)
            rd_i_flagsbuffer_set_flow(ctx->worker.segment->flags, idx);
        else if(ctx->worker.current.kind == RD_WI_CALL)
            rd_i_flagsbuffer_set_func(ctx->worker.segment->flags, idx);
        else if(ctx->worker.current.kind == RD_WI_JUMP)
            rd_i_flagsbuffer_set_jmpdst(ctx->worker.segment->flags, idx);

        if(ctx->worker.current.name) {
            rd_i_set_name(ctx, ctx->worker.current.address,
                          ctx->worker.current.name,
                          ctx->worker.current.confidence);
        }

        if(instr.delay_slots && !rd_is_delay_slot(&instr))
            _rd_engine_execute_delay_slots(ctx, &instr);
    }

done:
    if(ctx->worker.current.name) free(ctx->worker.current.name);
    return instr.length;
}

void rd_flow(RDContext* ctx, RDAddress address) {
    const RDSegmentFull* seg = ctx->worker.segment;

    // avoid inter-segment flow
    if(address < seg->base.start_address || address >= seg->base.end_address)
        return;

    ctx->worker.flow = (RDWorkerItem){
        .kind = RD_WI_FLOW,
        .address = address,
    };
}

bool rd_decode(RDContext* ctx, RDAddress address, RDInstruction* instr) {
    const RDSegmentFull* seg = rd_i_find_segment(ctx, address);
    if(!seg || !(seg->base.perm & RD_SP_X)) return false;

    *instr = (RDInstruction){0};
    return rd_i_engine_decode(ctx, address, instr);
}

bool rd_decode_prev(RDContext* ctx, RDAddress address, RDInstruction* instr) {
    const RDSegmentFull* seg = rd_i_find_segment(ctx, address);
    if(!seg || !(seg->base.perm & RD_SP_X)) return false;

    usize idx = rd_i_address2index(seg, address);
    if(!idx) return false;

    idx--;
    while(idx > 0 && rd_flagsbuffer_has_tail(seg->flags, idx))
        idx--;

    if(rd_flagsbuffer_has_tail(seg->flags, idx)) return false;

    *instr = (RDInstruction){0};
    return rd_i_engine_decode(ctx, rd_i_index2address(seg, idx), instr);
}
