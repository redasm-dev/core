#include "segment.h"
#include "core/context.h"
#include "io/buffer.h"
#include "io/flagsbuffer.h"
#include "support/stringpool.h"
#include <assert.h>
#include <redasm/allocator.h>
#include <redasm/segment.h>
#include <redasm/support/logging.h>

usize rd_i_address2index(const RDSegment* seg, RDAddress addr) {
    assert(addr >= rd_segment_get_start(seg) &&
           addr < rd_segment_get_end(seg) && "address out of range");
    return addr - rd_segment_get_start(seg);
}

RDAddress rd_i_index2address(const RDSegment* seg, usize idx) {
    RDAddress addr = rd_segment_get_start(seg) + (RDAddress)idx;

    assert(addr >= rd_segment_get_start(seg) &&
           addr < rd_segment_get_end(seg) && "index out of range");
    return addr;
}

const RDFlagsBuffer* rd_segment_get_flags(const RDSegment* self) {
    return self->flags;
}

const char* rd_segment_get_name(const RDSegment* self) { return self->name; }
RDSegmentPerm rd_segment_get_perm(const RDSegment* self) { return self->perm; }

bool rd_segment_has_perm(const RDSegment* self, RDSegmentPerm p) {
    return self && (self->perm & p);
}

RDAddress rd_segment_get_start(const RDSegment* self) {
    return rd_i_abs(self->context, self->rel_start);
}

RDAddress rd_segment_get_end(const RDSegment* self) {
    return rd_i_abs(self->context, self->rel_end);
}

usize rd_segment_get_size(const RDSegment* self) {
    return self->rel_end - self->rel_start;
}

RDSegment* rd_i_segment_create(RDContext* ctx, const char* name,
                               RDRelAddress addr, RDRelAddress endaddr,
                               u32 perm) {
    assert(name);

    if(addr >= endaddr) {
        RD_LOG_FAIL("invalid address range for segment '%s'", name);
        return NULL;
    }

    RDSegment* s = rd_alloc(sizeof(*s));

    *s = (RDSegment){
        .context = ctx,
        .name = rd_i_strpool_intern(&ctx->strings, name),
        .rel_start = addr,
        .rel_end = endaddr,
        .perm = perm,
        .flags = rd_i_flagsbuffer_create(endaddr - addr),
        .ovl_flags = rd_alloc0(endaddr - addr, sizeof(*s->ovl_flags)),
    };

    return s;
}

void rd_i_segment_destroy(RDSegment* self) {
    rd_i_buffer_destroy((RDBuffer*)self->flags);
    rd_free(self->ovl_flags);
    rd_free(self);
}

void rd_i_segment_set_ovl(const RDSegment* self, usize idx, RDFlagsOverlay f) {
    usize n = self->rel_end - self->rel_start;
    if(idx < n) self->ovl_flags[idx] |= (u8)f;
}

bool rd_i_segment_has_ovl(const RDSegment* self, usize idx, RDFlagsOverlay f) {
    usize n = self->rel_end - self->rel_start;
    return idx < n ? (self->ovl_flags[idx] & f) : false;
}

void rd_i_segment_clear_ovl(const RDSegment* self, usize idx,
                            RDFlagsOverlay f) {
    usize n = self->rel_end - self->rel_start;
    if(idx < n) self->ovl_flags[idx] &= (u8)~f;
}

bool rd_segment_set_bank(RDSegment* self, usize id, RDRelAddress base,
                         RDContext* ctx) {
    if(!self) return false;

    if(base > self->rel_start) {
        rd_add_problem(
            ctx, rd_segment_get_start(self), rd_segment_get_start(self),
            "segment '%s': bank base %llx exceeds start %llx", self->name,
            (unsigned long long)base, (unsigned long long)self->rel_start);

        return false;
    }

    self->bank.id = id;
    self->bank.base = base;
    rd_i_db_set_segment_bank(ctx, self);
    return true;
}

bool rd_segment_get_bank(const RDSegment* self, usize* id, RDAddress* base,
                         const RDContext* ctx) {
    if(id) *id = self->bank.id;
    if(base) *base = rd_i_abs(ctx, self->bank.base);
    return rd_i_segment_is_banked(self);
}

bool rd_i_segment_resolve_same_window(const RDSegment* self, RDAddress value,
                                      RDAddress* address) {
    if(!self || !rd_i_segment_is_banked(self)) return false;

    RDAddress resolved = value + self->bank.base;
    if(rd_i_db_find_segment(self->context, resolved) != self) return false;

    if(address) *address = resolved;
    return true;
}
