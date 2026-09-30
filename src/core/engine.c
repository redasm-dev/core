#include "engine.h"
#include "core/context.h"
#include "core/worker.h"
#include "io/flagsbuffer.h"
#include "support/containers.h"
#include "support/error.h"
#include "support/scratch.h"

#define RD_ENGINE_QUEUE_SIZE 8192

static inline bool _rd_engine_is_dirty_kind(RDEngineItemKind k) {
    return k == RD_EI_DIRTY || k == RD_EI_CODE;
}

static inline void _rd_engine_enqueue_dirty(RDContext* ctx, RDAddress address,
                                            usize n, RDEngineItemKind kind) {
    const RDSegment* seg = rd_i_db_find_segment(ctx, address);
    if(!seg) return;

    RDEngineItem item = {
        .kind = kind,
        .address = address,
        .from = address,
        .n = n,
    };

    rd_i_registermap_init(&item.registers);
    queue_push(&ctx->engine.qdirty, item);
    rd_i_engine_mark_dirty(ctx);
}

static void _rd_engine_queue_drain(RDEngineQueue* q) {
    while(!queue_is_empty(q)) {
        RDEngineItem item;
        queue_pop(q, &item);
        hmap_destroy(&item.registers); // tolerates zeroed
    }

    queue_destroy(q);
}

static const char* _rd_engine_queue_name(RDEngineItemKind k) {
    switch(k) {
        case RD_EI_FLOW: return "FLOW";
        case RD_EI_CALL: return "CALL";
        case RD_EI_JUMP: return "JUMP";
        default: break;
    }

    return NULL;
}

static const RDSegment* _rd_engine_find_segment(const RDContext* ctx,
                                                RDAddress address) {
    const RDSegment* seg = ctx->engine.segment;

    if(!seg || !rd_i_segment_contains(seg, address))
        return rd_i_db_find_segment(ctx, address);

    return seg;
}

static void _rd_engine_check_promote(RDContext* ctx, RDAddress address,
                                     RDEngineItemKind kind, const char* type) {
    const RDSegment* seg = _rd_engine_find_segment(ctx, address);
    if(!seg) return;

    usize dstidx = rd_i_address2index(seg, address);
    if(rd_flagsbuffer_has_tail(seg->flags, dstidx)) return;

    rd_i_engine_promote_target(ctx, seg, dstidx, kind, type);

    if(rd_flagsbuffer_has_noret(seg->flags, dstidx))
        rd_i_set_noret(ctx, ctx->engine.current.address);
}

static bool _rd_engine_accept_address(RDContext* ctx, RDAddress address,
                                      RDEngineQueue* q) {
    // consecutive duplicate fast reject
    if(!queue_is_empty(q) && queue_peek_last(q).address == address)
        return false;

    // non-existing or non-executable segment
    const RDSegment* seg = _rd_engine_find_segment(ctx, address);
    if(!seg || !rd_segment_has_perm(seg, RD_SP_X)) return false;

    usize idx = rd_i_address2index(seg, address);

    if(rd_i_segment_has_queued(seg, idx)) return false;

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

    rd_i_segment_set_queued(seg, idx);
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
    const RDSegment* saved_seg = ctx->engine.segment;

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
                             "cannot decode delay slot #%u", nslot);
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
    ctx->engine.segment = saved_seg;

    // reinstate the correct continuation, suppressing what emulate set
    return ctx->engine.flow;
}
//
static RDEngineQueue* _rd_engine_pick_queue(RDContext* ctx) {
    while(!queue_is_empty(&ctx->engine.qdirty)) {
        if(queue_peek_first(&ctx->engine.qdirty).kind != RD_EI_NONE)
            return &ctx->engine.qdirty;

        queue_discard(&ctx->engine.qdirty);
    }

    if(!queue_is_empty(&ctx->engine.qcall)) return &ctx->engine.qcall;
    if(!queue_is_empty(&ctx->engine.qjump)) return &ctx->engine.qjump;
    return NULL;
}

// Makes the next item ctx->engine.current and sets ctx->engine.segment.
// false: nothing to run this tick (empty queues, pc outside any segment,
// non-executable segment).
static bool _rd_engine_next_item(RDContext* ctx) {
    if(ctx->engine.flow.has_value) {
        ctx->engine.current.address = optional_take(&ctx->engine.flow);
        ctx->engine.current.kind = RD_EI_FLOW;
        assert(ctx->engine.segment && "flow tick with no current segment");
        return true;
    }

    RDEngineQueue* q = _rd_engine_pick_queue(ctx);
    if(!q) return false;

    hmap_destroy(&ctx->engine.current.registers);
    queue_pop(q, &ctx->engine.current);

    ctx->engine.segment =
        _rd_engine_find_segment(ctx, ctx->engine.current.address);

    if(!ctx->engine.segment) {
        rd_i_add_problem(ctx, ctx->engine.current.address,
                         ctx->engine.current.address,
                         "program counter outside any segment");
        return false;
    }

    return rd_segment_has_perm(ctx->engine.segment, RD_SP_X);
}

// Is there anything to decode at current.address?
// false: the tick ends here and instr->length is what it returns (the existing
// length when the address was already decoded, 0 otherwise).
static bool _rd_engine_should_decode(RDContext* ctx, usize idx,
                                     RDInstruction* instr) {
    const RDEngineItem* cur = &ctx->engine.current;
    const RDSegment* seg = ctx->engine.segment;

    // something already re-decoded this range (eg. flow from a neighbour,
    // or a duplicate mark) there is nothing left to do
    if(_rd_engine_is_dirty_kind(cur->kind) &&
       !rd_flagsbuffer_has_unknown(seg->flags, idx))
        return false;

    if(rd_flagsbuffer_has_tail(seg->flags, idx)) {
        const char* queue_kind = _rd_engine_queue_name(cur->kind);
        assert(queue_kind && "invalid queue kind");

        rd_i_add_problem(ctx, cur->from, cur->address,
                         "%s target points into middle of existing instruction",
                         queue_kind);
        return false;
    }

    if(rd_flagsbuffer_has_code(seg->flags, idx)) {
        // queued as a call/jump target but flow decoded it first
        rd_i_engine_promote_target(ctx, seg, idx, cur->kind, cur->func_type);
        instr->length = (u16)rd_i_flagsbuffer_get_range_length(seg->flags, idx);
        return false;
    }

    return true;
}

// Decodes at current.address and classifies the bytes as code.
// false: nothing was classified (instr->length is 0).
static bool _rd_engine_do_decode(RDContext* ctx, usize idx,
                                 RDInstruction* instr) {
    const RDEngineItem* cur = &ctx->engine.current;
    const RDSegment* seg = ctx->engine.segment;

    if(!rd_i_engine_decode(ctx, cur->address, seg, idx, instr)) return false;
    assert(instr->length && "decode succeeded without a length");

    if(!rd_i_flagsbuffer_has_unknown_n(seg->flags, idx, instr->length)) {
        rd_i_add_problem(ctx, cur->address, cur->address,
                         "instruction overlaps existing item (length: %u)",
                         instr->length);
        instr->length = 0;
        return false;
    }

    if(!rd_i_flagsbuffer_set_code(seg->flags, idx, instr->length)) {
        rd_i_add_problem(ctx, cur->address, cur->address,
                         "failed to classify as code (length: %u)",
                         instr->length);
        instr->length = 0;
        return false;
    }

    return true;
}

static void _rd_engine_emulate(RDContext* ctx, RDInstruction* instr) {
    if(instr->delay_slots && !rd_instr_is_delay_slot(instr)) {
        RDEngineFlow dslot_flow = _rd_engine_execute_delay_slots(ctx, instr);

        ctx->processorplugin->emulate(ctx, instr, ctx->processor);

        // always override: branch's rd_flow points at first delay slot,
        // which is architecturally wrong. real PC after execution is
        // determined by the last delay slot's flow.
        ctx->engine.flow = dslot_flow;
    }
    else
        ctx->processorplugin->emulate(ctx, instr, ctx->processor);
}

static void _rd_engine_apply_flags(const RDSegment* seg, usize idx,
                                   const RDInstruction* instr) {
    if(rd_instr_is_jump(instr))
        rd_i_flagsbuffer_set_jump(seg->flags, idx);
    else if(rd_instr_is_call(instr))
        rd_i_flagsbuffer_set_call(seg->flags, idx);

    if(rd_instr_is_cond(instr)) rd_i_flagsbuffer_set_cond(seg->flags, idx);

    if(rd_instr_is_delay_slot(instr))
        rd_i_flagsbuffer_set_dslot(seg->flags, idx);

    if(instr->no_ret) rd_i_flagsbuffer_set_noret(seg->flags, idx);
}

static void _rd_engine_apply_arrival(RDContext* ctx, const RDSegment* seg,
                                     usize idx) {
    const RDEngineItem* cur = &ctx->engine.current;

    if(cur->kind == RD_EI_FLOW)
        rd_i_flagsbuffer_set_flow(seg->flags, idx);
    else
        rd_i_engine_promote_target(ctx, seg, idx, cur->kind, cur->func_type);
}

bool rd_i_engine_decode(RDContext* ctx, RDAddress address, const RDSegment* seg,
                        usize index, RDInstruction* instr) {
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
                    op->addr = (RDAddress)op->imm; // ?!?
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

    _rd_engine_check_promote(ctx, address, RD_EI_JUMP, NULL);
    return false;
}

bool rd_i_engine_enqueue_call(RDContext* ctx, RDAddress address,
                              const char* type) {
    if(_rd_engine_accept_address(ctx, address, &ctx->engine.qcall)) {
        RDEngineItem item = {
            .kind = RD_EI_CALL,
            .address = address,
            .from = ctx->engine.current.address,
            .func_type = rd_i_strpool_intern(&ctx->strings, type),
        };

        hmap_dup(&item.registers, &ctx->engine.current.registers);
        queue_push(&ctx->engine.qcall, item);
        return true;
    }

    _rd_engine_check_promote(ctx, address, RD_EI_CALL, type);
    return false;
}

void rd_i_engine_enqueue_dirty(RDContext* ctx, RDAddress address, usize n) {
    _rd_engine_enqueue_dirty(ctx, address, n, RD_EI_DIRTY);
}

void rd_i_engine_enqueue_code(RDContext* ctx, RDAddress address, usize n) {
    _rd_engine_enqueue_dirty(ctx, address, n, RD_EI_CODE);
}

bool rd_i_engine_promote_target(RDContext* ctx, const RDSegment* seg, usize idx,
                                RDEngineItemKind kind, const char* type) {
    if(!rd_segment_has_perm(seg, RD_SP_X) ||
       !rd_flagsbuffer_has_code(seg->flags, idx))
        return false;

    switch(kind) {
        case RD_EI_CALL: return rd_i_function_declare_if(ctx, seg, idx, type);

        case RD_EI_JUMP:
            rd_i_flagsbuffer_set_jmpdst(seg->flags, idx);
            return false;

        default: break; // FLOW, CODE, DIRTY: nothing to promote
    }

    return false;
}

bool rd_i_engine_mark_dirty(RDContext* ctx) {
    // rewind already done and waiting to be driven: still dirty, nothing to do
    if(ctx->engine.step == RD_WS_RECONCILE) return true;

    if(ctx->engine.step < RD_WS_DONE) return false; // pipeline still running
    ctx->engine.step = RD_WS_RECONCILE;
    return true;
}

bool rd_i_engine_has_pending_code(const RDContext* ctx) {
    return (ctx->engine.flow.has_value || !queue_is_empty(&ctx->engine.qjump) ||
            !queue_is_empty(&ctx->engine.qcall) ||
            !queue_is_empty(&ctx->engine.qdirty));
}

u16 rd_i_engine_tick(RDContext* ctx) {
    RDInstruction instr = {
        .delay_slots = ctx->engine.dslot_info.n ? RD_IS_DSLOT : 0,
    };

    if(!_rd_engine_next_item(ctx)) return 0;

    assert(ctx->engine.current.registers.hash &&
           "invalid registers hash function");
    assert(ctx->engine.current.registers.equal &&
           "invalid registers equal function");

    usize idx =
        rd_i_address2index(ctx->engine.segment, ctx->engine.current.address);
    rd_i_segment_clear_queued(ctx->engine.segment, idx);

    if(!_rd_engine_should_decode(ctx, idx, &instr)) return instr.length;
    if(!_rd_engine_do_decode(ctx, idx, &instr)) return instr.length;

    _rd_engine_emulate(ctx, &instr);
    _rd_engine_apply_flags(ctx->engine.segment, idx, &instr);
    _rd_engine_apply_arrival(ctx, ctx->engine.segment, idx);

    return instr.length;
}

void rd_flow(RDContext* ctx, RDAddress address) {
    // unset and do checks
    optional_unset(&ctx->engine.flow);

    const RDSegment* seg = ctx->engine.segment;

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
    if(!ctx || !buf) return false;

    rd_scratch_clear(buf);

    const RDProcessorPlugin* plugin = ctx->processorplugin;

    if(!plugin->encode) {
        rd_format_to(buf, "processor '%s' does not support encoding",
                     plugin->id);
        return false;
    }

    return plugin->encode(ctx, address, s, buf, ctx->processor);
}

bool rd_decode(RDContext* ctx, RDAddress address, RDInstruction* instr) {
    if(!ctx) return false;

    const RDSegment* seg = _rd_engine_find_segment(ctx, address);
    if(!seg || !rd_segment_has_perm(seg, RD_SP_X)) return false;

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
    const RDSegment* seg = _rd_engine_find_segment(ctx, address);
    if(!seg || !rd_segment_has_perm(seg, RD_SP_X)) return false;

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

void rd_i_engine_init(RDContext* ctx) {
    rd_i_registermap_init(&ctx->engine.current.registers);
    queue_reserve(&ctx->engine.qdirty, RD_ENGINE_QUEUE_SIZE);
    queue_reserve(&ctx->engine.qcall, RD_ENGINE_QUEUE_SIZE);
    queue_reserve(&ctx->engine.qjump, RD_ENGINE_QUEUE_SIZE);
}

void rd_i_engine_destroy(RDContext* ctx) {
    _rd_engine_queue_drain(&ctx->engine.qdirty);
    _rd_engine_queue_drain(&ctx->engine.qcall);
    _rd_engine_queue_drain(&ctx->engine.qjump);
}
