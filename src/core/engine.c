#include "engine.h"
#include "core/context.h"
#include "io/flagsbuffer.h"
#include "support/containers.h"
#include "support/error.h"
#include "support/scratch.h"

static const char* _rd_engine_queue_name(RDEngineItemKind k) {
    switch(k) {
        case RD_EI_FLOW: return "FLOW";
        case RD_EI_CALL: return "CALL";
        case RD_EI_JUMP: return "JUMP";
        default: break;
    }

    return NULL;
}

static const RDSegmentFull* _rd_engine_find_segment(const RDContext* ctx,
                                                    RDAddress address) {
    const RDSegmentFull* seg = ctx->engine.segment;

    if(!seg || !rd_i_segment_contains(seg, address))
        return rd_i_db_find_segment(ctx, address);

    return seg;
}

static bool _rd_engine_accept_address(RDContext* ctx, RDAddress address,
                                      RDEngineQueue* q) {
    // consecutive duplicate fast reject
    if(!queue_is_empty(q) && queue_peek_last(q).address == address)
        return false;

    // non-existing or non-executable segment
    const RDSegmentFull* seg = _rd_engine_find_segment(ctx, address);
    if(!seg || !(seg->base.perm & RD_SP_X)) return false;

    usize idx = rd_i_address2index(seg, address);

    if(rd_i_flagsbuffer_has_queued(seg->flags, idx)) return false;

    // tail = pointing into the middle of an existing instruction:
    // plugin bug or deliberate obfuscation: reject and surface as problem
    if(rd_flagsbuffer_has_tail(seg->flags, idx)) {
        rd_i_add_problem(ctx, address, address,
                         "jump/call target points into the middle of an "
                         "existing instruction");
        return false;
    }

    // already fully decoded at this boundary: tick would be a no-op
    if(rd_flagsbuffer_has_code(seg->flags, idx)) return false;

    rd_i_flagsbuffer_set_queued(seg->flags, idx);
    return true;
}

static RDEngineFlow _rd_engine_execute_delay_slots(RDContext* ctx,
                                                   const RDInstruction* instr) {
    // save outer tick identity
    // - flow is intentionally NOT saved: the last delay slot's rd_flow call
    //   sets the continuation address that the branch emulate needs to see
    // - registers are intentionally NOT saved: delay slot writes must be
    //   visible to branch emulate
    RDAddress saved_address = ctx->engine.current.address;
    RDEngineItemKind saved_kind = ctx->engine.current.kind;
    RDConfidence saved_conf = ctx->engine.current.confidence;
    char* saved_name = ctx->engine.current.name;
    const RDSegmentFull* saved_seg = ctx->engine.segment;

    ctx->engine.dslot_info.instr = *instr;
    ctx->engine.dslot_info.n = 0;
    RDAddress address = instr->address + instr->length;
    u8 nslot = 1;

    while(nslot <= ctx->engine.dslot_info.instr.delay_slots) {
        rd_flow(ctx, address);

        ctx->engine.dslot_info.n = nslot; // inside slot tick
        u32 len = rd_i_engine_tick(ctx);
        ctx->engine.dslot_info.n = 0; // back out

        if(!len) {
            rd_i_add_problem(ctx, instr->address, address,
                             "cannot decode delay slot #%u",
                             ctx->engine.dslot_info.n);
            break;
        }

        address += len;
        nslot++;
    }

    // clear delay slot state
    ctx->engine.dslot_info = (RDDelaySlotInfo){0};

    // restore outer tick identity: flow deliberately excluded
    ctx->engine.current.address = saved_address;
    ctx->engine.current.kind = saved_kind;
    ctx->engine.current.confidence = saved_conf;
    ctx->engine.current.name = saved_name;
    ctx->engine.segment = saved_seg;

    // reinstate the correct continuation, suppressing what emulate set
    return ctx->engine.flow;
}

bool rd_i_engine_decode(RDContext* ctx, RDAddress address,
                        const RDSegmentFull* seg, usize index,
                        RDInstruction* instr) {
    assert(!rd_flagsbuffer_has_tail(seg->flags, index));

    instr->address = address;
    ctx->processorplugin->decode(ctx, instr, ctx->processor);

    if(instr->length > 0) {
        if(rd_i_flagsbuffer_has_op_over(seg->flags, index)) {
            const RDOvrOperandVect* ovr_ops =
                rd_i_db_get_all_ovr_operand(ctx, address);

            const RDOvrOperand* ovr_op;
            vect_each(ovr_op, ovr_ops) {
                assert(ovr_op->index < RD_MAX_OPERANDS);

                RDOperand* op = &instr->operands[ovr_op->index];

                if(op->kind == RD_OP_IMM) {
                    op->kind = RD_OP_ADDR;
                    op->addr = op->imm;
                }
                else {
                    panic("instruction @ %x operand %d, invalid override",
                          ovr_op->index, address);
                }
            }
        }

        return true;
    }

    return false;
}

bool rd_i_engine_enqueue_jump(RDContext* ctx, RDAddress address) {
    if(_rd_engine_accept_address(ctx, address, &ctx->engine.qjump)) {
        RDEngineItem item = {
            .kind = RD_EI_JUMP,
            .address = address,
            .from = ctx->engine.current.address,
        };

        hmap_dup(&item.registers, &ctx->engine.current.registers);
        queue_push(&ctx->engine.qjump, item);
        return true;
    }

    // may be already code (backward jump / loop). Promote FL_JMPDST if so.
    // tail and non-executable cases are already handled inside accept_address.
    const RDSegmentFull* seg = _rd_engine_find_segment(ctx, address);
    if(!seg) return false;

    usize dstidx = rd_i_address2index(seg, address);
    if(rd_flagsbuffer_has_tail(seg->flags, dstidx)) return false;

    if((seg->base.perm & RD_SP_X) &&
       rd_flagsbuffer_has_code(seg->flags, dstidx)) {
        rd_i_flagsbuffer_set_jmpdst(seg->flags, dstidx);
    }

    if(rd_flagsbuffer_has_noret(seg->flags, dstidx))
        rd_i_set_noret(ctx, ctx->engine.current.address);

    return false;
}

bool rd_i_engine_enqueue_call(RDContext* ctx, RDAddress address,
                              const char* name, RDConfidence c) {
    if(_rd_engine_accept_address(ctx, address, &ctx->engine.qcall)) {
        RDEngineItem item = {
            .kind = RD_EI_CALL,
            .confidence = c,
            .address = address,
            .from = ctx->engine.current.address,
            .name = name ? rd_strdup(name) : NULL,
        };

        hmap_dup(&item.registers, &ctx->engine.current.registers);
        queue_push(&ctx->engine.qcall, item);
        return true;
    }

    const RDSegmentFull* seg = _rd_engine_find_segment(ctx, address);
    if(!seg) return false;

    usize dstidx = rd_i_address2index(seg, address);
    if(rd_flagsbuffer_has_tail(seg->flags, dstidx)) return false;

    if((seg->base.perm & RD_SP_X) &&
       rd_flagsbuffer_has_code(seg->flags, dstidx)) {
        rd_i_flagsbuffer_set_func(seg->flags, dstidx);
        if(name) rd_i_set_name(ctx, address, name, c);
    }

    if(rd_flagsbuffer_has_noret(seg->flags, dstidx))
        rd_i_set_noret(ctx, ctx->engine.current.address);

    return false;
}

bool rd_i_engine_has_pending_code(const RDContext* ctx) {
    return (ctx->engine.flow.has_value || !queue_is_empty(&ctx->engine.qjump) ||
            !queue_is_empty(&ctx->engine.qcall));
}

u16 rd_i_engine_tick(RDContext* ctx) {
    RDInstruction instr = {
        .delay_slots = ctx->engine.dslot_info.n ? RD_IS_DSLOT : 0,
    };

    if(ctx->engine.flow.has_value) {
        ctx->engine.current.address = optional_take(&ctx->engine.flow);
        ctx->engine.current.kind = RD_EI_FLOW;
        ctx->engine.current.confidence = RD_CONFIDENCE_AUTO;
        ctx->engine.current.name = NULL;
        assert(ctx->engine.segment && "flow tick with no current segment");
    }
    else {
        if(!queue_is_empty(&ctx->engine.qcall)) {
            hmap_destroy(&ctx->engine.current.registers);
            queue_pop(&ctx->engine.qcall, &ctx->engine.current);
        }
        else if(!queue_is_empty(&ctx->engine.qjump)) {
            hmap_destroy(&ctx->engine.current.registers);
            queue_pop(&ctx->engine.qjump, &ctx->engine.current);
        }
        else
            return 0;

        ctx->engine.segment =
            _rd_engine_find_segment(ctx, ctx->engine.current.address);

        if(!ctx->engine.segment) {
            rd_i_add_problem(ctx, ctx->engine.current.address,
                             ctx->engine.current.address,
                             "program counter outside any segment");
            goto done;
        }

        if(!(ctx->engine.segment->base.perm & RD_SP_X)) goto done;
    }

    assert(ctx->engine.current.registers.hash &&
           "invalid registers hash function");
    assert(ctx->engine.current.registers.equal &&
           "invalid registers equal function");

    usize idx =
        rd_i_address2index(ctx->engine.segment, ctx->engine.current.address);

    rd_i_flagsbuffer_undefine_queued(ctx->engine.segment->flags, idx);

    if(rd_flagsbuffer_has_tail(ctx->engine.segment->flags, idx)) {
        const char* queue_kind =
            _rd_engine_queue_name(ctx->engine.current.kind);
        assert(queue_kind && "invalid queue kind");

        rd_i_add_problem(
            ctx, ctx->engine.current.from, ctx->engine.current.address,
            "%s target points into middle of existing instruction", queue_kind);

        goto done;
    }

    if(rd_flagsbuffer_has_code(ctx->engine.segment->flags, idx)) {
        instr.length = (u16)rd_i_flagsbuffer_get_range_length(
            ctx->engine.segment->flags, idx);
        goto done;
    }

    if(!rd_i_engine_decode(ctx, ctx->engine.current.address,
                           ctx->engine.segment, idx, &instr))
        goto done;

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

        if(instr.delay_slots && !rd_instr_is_delay_slot(&instr)) {
            RDEngineFlow dslot_flow =
                _rd_engine_execute_delay_slots(ctx, &instr);

            ctx->processorplugin->emulate(ctx, &instr, ctx->processor);

            // always override: branch's rd_flow points at first delay slot,
            // which is architecturally wrong. real PC after execution is
            // determined by the last delay slot's flow.
            ctx->engine.flow = dslot_flow;
        }
        else
            ctx->processorplugin->emulate(ctx, &instr, ctx->processor);

        if(rd_instr_is_jump(&instr))
            rd_i_flagsbuffer_set_jump(ctx->engine.segment->flags, idx);
        else if(rd_instr_is_call(&instr))
            rd_i_flagsbuffer_set_call(ctx->engine.segment->flags, idx);

        if(rd_instr_is_cond(&instr))
            rd_i_flagsbuffer_set_cond(ctx->engine.segment->flags, idx);

        if(rd_instr_is_delay_slot(&instr))
            rd_i_flagsbuffer_set_dslot(ctx->engine.segment->flags, idx);

        if(instr.no_ret)
            rd_i_flagsbuffer_set_noret(ctx->engine.segment->flags, idx);

        if(ctx->engine.current.kind == RD_EI_FLOW)
            rd_i_flagsbuffer_set_flow(ctx->engine.segment->flags, idx);
        else if(ctx->engine.current.kind == RD_EI_CALL)
            rd_i_flagsbuffer_set_func(ctx->engine.segment->flags, idx);
        else if(ctx->engine.current.kind == RD_EI_JUMP)
            rd_i_flagsbuffer_set_jmpdst(ctx->engine.segment->flags, idx);

        if(ctx->engine.current.name) {
            rd_i_set_name(ctx, ctx->engine.current.address,
                          ctx->engine.current.name,
                          ctx->engine.current.confidence);
        }
    }

done:
    if(ctx->engine.current.name) rd_free(ctx->engine.current.name);
    return instr.length;
}

void rd_flow(RDContext* ctx, RDAddress address) {
    // unset and do checks
    optional_unset(&ctx->engine.flow);

    const RDSegmentFull* seg = ctx->engine.segment;

    // don't falltrough NORET locations
    usize curr_idx = rd_i_address2index(seg, ctx->engine.current.address);
    if(rd_flagsbuffer_has_noret(seg->flags, curr_idx)) return;

    // avoid inter-segment flow
    if(!rd_i_segment_contains(seg, address)) return;

    usize flow_idx = rd_i_address2index(seg, address);

    if(rd_flagsbuffer_has_tail(seg->flags, flow_idx)) {
        rd_i_add_problem(ctx, address, address,
                         "flow into the middle of an existing instruction "
                         "(processor plugin bug: wrong instruction length?)");
        return;
    }

    if(rd_flagsbuffer_has_data(seg->flags, flow_idx)) {
        rd_i_add_problem(ctx, address, address, "flow into data region");
        return;
    }

    if(rd_flagsbuffer_has_code(seg->flags, flow_idx)) {
        rd_i_flagsbuffer_set_flow(seg->flags, flow_idx);
        return;
    }

    if(ctx->engine.dslot_info.n &&
       ctx->engine.dslot_info.n == ctx->engine.dslot_info.instr.delay_slots &&
       !rd_instr_can_flow(&ctx->engine.dslot_info.instr))
        return;

    // address accepted, flow there
    optional_set(&ctx->engine.flow, address);
    ctx->engine.current.from = ctx->engine.current.address;
}

bool rd_encode(RDContext* ctx, RDAddress address, const char* s,
               RDScratchBuffer* buf) {
    if(!buf) return false;

    rd_scratch_clear(buf);

    const RDProcessorPlugin* plugin = ctx->processorplugin;

    if(!plugin->encode) {
        rd_i_format(&buf->impl, "processor '%s' does not support encoding",
                    plugin->id);
        return false;
    }

    return plugin->encode(ctx, address, s, buf, ctx->processor);
}

bool rd_decode(RDContext* ctx, RDAddress address, RDInstruction* instr) {
    const RDSegmentFull* seg = _rd_engine_find_segment(ctx, address);
    if(!seg || !(seg->base.perm & RD_SP_X)) return false;

    usize idx = rd_i_address2index(seg, address);
    if(rd_flagsbuffer_has_tail(seg->flags, idx)) return false;

    *instr = (RDInstruction){0};
    ctx->engine.segment = seg;
    return rd_i_engine_decode(ctx, address, seg, idx, instr);
}

bool rd_decode_n(RDContext* ctx, RDAddress address, RDInstruction* instrs,
                 usize n) {
    for(usize i = 0; i < n; i++) {
        if(!rd_decode(ctx, address, &instrs[i])) return false;
        address += instrs[i].length;
    }

    return true;
}

bool rd_decode_prev(RDContext* ctx, RDAddress address, RDInstruction* instr) {
    const RDSegmentFull* seg = _rd_engine_find_segment(ctx, address);
    if(!seg || !(seg->base.perm & RD_SP_X)) return false;

    usize idx = rd_i_address2index(seg, address);
    if(!idx) return false;

    idx--;
    while(idx > 0 && rd_flagsbuffer_has_tail(seg->flags, idx))
        idx--;

    if(rd_flagsbuffer_has_tail(seg->flags, idx)) return false;

    *instr = (RDInstruction){0};
    ctx->engine.segment = seg;
    return rd_i_engine_decode(ctx, rd_i_index2address(seg, idx), seg, idx,
                              instr);
}

void rd_i_engine_destroy(RDContext* ctx) {
    while(!queue_is_empty(&ctx->engine.qcall)) {
        RDEngineItem item;
        queue_pop(&ctx->engine.qcall, &item);
        rd_free(item.name);
    }

    queue_destroy(&ctx->engine.qcall);
    queue_destroy(&ctx->engine.qjump);
}
