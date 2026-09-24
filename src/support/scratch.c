#include "scratch.h"
#include "core/state.h"
#include "support/containers.h"
#include <redasm/allocator.h>
#include <redasm/support/logging.h>
#include <redasm/support/scratch.h>
#include <stdarg.h>

RDScratchBuffer* rd_scratch_create(void) {
    return (RDScratchBuffer*)rd_alloc0(1, sizeof(RDScratchBuffer));
}

void rd_scratch_destroy(RDScratchBuffer* self) {
    if(!self) return;

    vect_destroy(&self->impl);
    rd_free(self);
}

void rd_scratch_resize(RDScratchBuffer* self, usize n) {
    vect_resize(&self->impl, n);
}

void rd_scratch_reserve(RDScratchBuffer* self, usize n) {
    usize oldcap = vect_capacity(&self->impl);
    if(n <= oldcap) return;

    vect_reserve(&self->impl, n);
    memset(self->impl.data + oldcap, 0, vect_capacity(&self->impl) - oldcap);
}

bool rd_scratch_write(RDScratchBuffer* self, usize idx, const void* data,
                      usize n) {
    if(!n) return true;

    if(!data) {
        RD_LOG_FAIL("rd_scratch_write: NULL data");
        return false;
    }

    usize cap = vect_capacity(&self->impl);

    if(idx > cap || n > cap - idx) { // overflow-safe
        RD_LOG_FAIL("rd_scratch_write: [%zu, %zu) exceeds capacity %zu", idx,
                    idx + n, cap);
        return false;
    }

    memcpy(self->impl.data + idx, data, n);

    usize end = idx + n;
    if(end > self->impl.length) self->impl.length = end;

    return true;
}

void rd_scratch_concat(RDScratchBuffer* dst, const RDScratchBuffer* src) {
    rd_scratch_append(dst, rd_scratch_data(src), rd_scratch_length(src));
}

void rd_scratch_concat_n(RDScratchBuffer* dst, const RDScratchBuffer* src,
                         usize n) {
    usize avail = rd_scratch_length(src);

    if(n > avail) {
        RD_LOG_WARN("n (%zu) exceeds source length (%zu), clamping", n, avail);
        n = avail;
    }

    rd_scratch_append(dst, rd_scratch_data(src), n);
}

void rd_scratch_append(RDScratchBuffer* self, const void* data, usize n) {
    if(!data || !n) return;
    rd_scratch_reserve(self, self->impl.length + n);
    rd_scratch_write(self, self->impl.length, data, n);
}

void rd_scratch_push(RDScratchBuffer* self, char c) {
    vect_push(&self->impl, c);
}

bool rd_scratch_is_empty(const RDScratchBuffer* self) {
    return vect_is_empty(&self->impl);
}

bool rd_scratch_get(const RDScratchBuffer* self, usize idx, char* c) {
    if(idx >= self->impl.length) return false;

    if(c) *c = self->impl.data[idx];
    return true;
}

bool rd_scratch_set(RDScratchBuffer* self, usize idx, char c) {
    if(idx >= self->impl.length) return false;

    self->impl.data[idx] = c;
    return true;
}

void rd_scratch_clear(RDScratchBuffer* self) { str_clear(&self->impl); }

void rd_scratch_putf(RDScratchBuffer* self, const char* fmt, ...) {
    va_list args; // format in a temporary buffer, then append
    va_start(args, fmt);
    const char* result = rd_i_vformat(&rd_i_state.scratch_buf, fmt, args);
    va_end(args);

    rd_scratch_puts(self, result);
}

void rd_scratch_puts(RDScratchBuffer* self, const char* s) {
    if(s) str_append(&self->impl, s);
}

void rd_scratch_puts_n(RDScratchBuffer* self, const char* s, usize n) {
    if(s) str_append_n(&self->impl, s, n);
}

void rd_scratch_putchar(RDScratchBuffer* self, char c) {
    str_push(&self->impl, c);
}

const char* rd_scratch_first(const RDScratchBuffer* self) {
    return vect_is_empty(&self->impl) ? NULL : vect_first(&self->impl);
}

const char* rd_scratch_last(const RDScratchBuffer* self) {
    return vect_is_empty(&self->impl) ? NULL : vect_last(&self->impl);
}

const char* rd_scratch_data(const RDScratchBuffer* self) {
    return self->impl.data ? self->impl.data : "";
}

usize rd_scratch_length(const RDScratchBuffer* self) {
    return self->impl.length;
}

usize rd_scratch_capacity(const RDScratchBuffer* self) {
    return self->impl.capacity;
}
