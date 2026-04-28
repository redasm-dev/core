#include "engine.h"
#include "core/context.h"
#include "io/flagsbuffer.h"
#include "support/containers.h"

static bool _rd_engine_accept_address(const RDContext* ctx, RDAddress address,
                                      RDWorkerQueue* q) {
    if(!queue_is_empty(q) && queue_peek_back(q).address == address)
        return false;

    const RDSegmentFull* seg = ctx->engine.segment;

    if(!seg || address < seg->base.start_address ||
       address >= seg->base.end_address)
        seg = rd_i_find_segment(ctx, address);

    return seg && (seg->base.perm & RD_SP_X);
}

static void _rd_engine_execute_delay_slots(RDContext* ctx,
                                           const RDInstruction* instr) {
    ctx->engine.dslot_info.instr = *instr;
    ctx->engine.dslot_info.n = 1;
    RDAddress address = instr->address + instr->length;

    while(ctx->engine.dslot_info.n <=
          ctx->engine.dslot_info.instr.delay_slots) {
        rd_flow(ctx, address);
        u32 len = rd_i_engine_tick(ctx);

        if(!len) {
            rd_i_add_problem(ctx, instr->address, address,
                             "cannot decode delay slot #%u",
                             ctx->engine.dslot_info.n);
            break;
        }

        address += len;
        ctx->engine.dslot_info.n++;
    }

    ctx->engine.dslot_info = (RDDelaySlotInfo){0};
}

bool rd_i_engine_decode(RDContext* ctx, RDAddress address,
                        RDInstruction* instr) {
    assert(ctx->processorplugin->decode && "missing instruction decoder");
    instr->address = address;
    ctx->processorplugin->decode(ctx, instr, ctx->processor);
    return instr->length > 0;
}

bool rd_i_engine_enqueue_jump(RDContext* ctx, RDAddress addr) {
    if(_rd_engine_accept_address(ctx, addr, &ctx->engine.qjump)) {
        queue_push(&ctx->engine.qjump, (RDWorkerItem){
                                           .kind = RD_WI_JUMP,
                                           .address = addr,
                                       });
        return true;
    }

    return false;
}

bool rd_i_engine_enqueue_call(RDContext* ctx, RDAddress addr, const char* name,
                              RDConfidence c) {
    if(_rd_engine_accept_address(ctx, addr, &ctx->engine.qcall)) {
        queue_push(&ctx->engine.qcall,
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
           (ctx->engine.flow.kind == RD_WI_FLOW ||
            !queue_is_empty(&ctx->engine.qjump) ||
            !queue_is_empty(&ctx->engine.qcall));
}

u16 rd_i_engine_tick(RDContext* ctx) {
    if(ctx->engine.flow.kind == RD_WI_FLOW) {
        ctx->engine.current = ctx->engine.flow;
        ctx->engine.flow.kind = RD_WI_NONE;
    }
    else if(!queue_is_empty(&ctx->engine.qcall)) {
        queue_pop(&ctx->engine.qcall, &ctx->engine.current);
    }
    else if(!queue_is_empty(&ctx->engine.qjump)) {
        queue_pop(&ctx->engine.qjump, &ctx->engine.current);
    }
    else
        return 0;

    ctx->engine.segment = rd_i_find_segment(ctx, ctx->engine.current.address);
    assert(ctx->engine.segment && "program counter without segment");
    if(!(ctx->engine.segment->base.perm & RD_SP_X)) goto done;

    usize idx =
        rd_i_address2index(ctx->engine.segment, ctx->engine.current.address);

    RDInstruction instr = {
        .delay_slots = ctx->engine.dslot_info.n ? RD_IS_DSLOT : 0,
    };

    if(rd_flagsbuffer_has_code(ctx->engine.segment->flags, idx)) {
        instr.length =
            rd_i_flagsbuffer_get_range_length(ctx->engine.segment->flags, idx);

        if(ctx->engine.current.kind == RD_WI_JUMP) {
            // loops case: destination gets decoded before source
            rd_i_flagsbuffer_set_jmpdst(ctx->engine.segment->flags, idx);
        }
        else if(ctx->engine.current.kind == RD_WI_CALL) {
            // reclassified locations as function
            rd_i_flagsbuffer_set_func(ctx->engine.segment->flags, idx);
        }

        if(ctx->engine.current.name) {
            rd_i_set_name(ctx, ctx->engine.current.address,
                          ctx->engine.current.name,
                          ctx->engine.current.confidence);
        }

        goto done;
    }

    if(!rd_i_engine_decode(ctx, ctx->engine.current.address, &instr)) goto done;

    if(instr.length) {
        if(!rd_i_flagsbuffer_has_unknown_n(ctx->engine.segment->flags, idx,
                                           instr.length)) {
            rd_i_add_problem(ctx, ctx->engine.current.address,
                             ctx->engine.current.address,
                             "instruction overlaps existing item (length: %u)",
                             instr.length);
            instr.length = 0;
            goto done;
        }

        if(!rd_i_flagsbuffer_set_code(ctx->engine.segment->flags, idx,
                                      instr.length)) {
            rd_i_add_problem(
                ctx, ctx->engine.current.address, ctx->engine.current.address,
                "failed to classify as code (length: %u)", instr.length);
            instr.length = 0;
            goto done;
        }

        const RDProcessorPlugin* plugin = ctx->processorplugin;
        assert(plugin->emulate);
        plugin->emulate(ctx, &instr, ctx->processor);

        if(instr.flow == RD_IF_JUMP || instr.flow == RD_IF_JUMP_COND)
            rd_i_flagsbuffer_set_jump(ctx->engine.segment->flags, idx);
        if(instr.flow == RD_IF_CALL || instr.flow == RD_IF_CALL_COND)
            rd_i_flagsbuffer_set_call(ctx->engine.segment->flags, idx);

        if(instr.flow == RD_IF_JUMP_COND || instr.flow == RD_IF_CALL_COND)
            rd_i_flagsbuffer_set_cond(ctx->engine.segment->flags, idx);

        if(ctx->engine.current.kind == RD_WI_FLOW)
            rd_i_flagsbuffer_set_flow(ctx->engine.segment->flags, idx);
        else if(ctx->engine.current.kind == RD_WI_CALL)
            rd_i_flagsbuffer_set_func(ctx->engine.segment->flags, idx);
        else if(ctx->engine.current.kind == RD_WI_JUMP)
            rd_i_flagsbuffer_set_jmpdst(ctx->engine.segment->flags, idx);

        if(ctx->engine.current.name) {
            rd_i_set_name(ctx, ctx->engine.current.address,
                          ctx->engine.current.name,
                          ctx->engine.current.confidence);
        }

        if(instr.delay_slots && !rd_is_delay_slot(&instr))
            _rd_engine_execute_delay_slots(ctx, &instr);
    }

done:
    if(ctx->engine.current.name) rd_free(ctx->engine.current.name);
    return instr.length;
}

void rd_flow(RDContext* ctx, RDAddress address) {
    const RDSegmentFull* seg = ctx->engine.segment;

    // avoid inter-segment flow
    if(address < seg->base.start_address || address >= seg->base.end_address)
        return;

    ctx->engine.flow = (RDWorkerItem){
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

void rd_i_engine_destroy(RDContext* ctx) {
    while(!queue_is_empty(&ctx->engine.qcall)) {
        RDWorkerItem item;
        queue_pop(&ctx->engine.qcall, &item);
        rd_free(item.name);
    }

    queue_destroy(&ctx->engine.qcall);
    queue_destroy(&ctx->engine.qjump);
}
