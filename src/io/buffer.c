#include "buffer.h"
#include "support/containers.h"
#include <stdlib.h>
#include <string.h>

#define RD_STR_BUF_MIN_CAPACITY 1024

static inline RDByteBuffer* _rd_as_bytebuffer(RDBuffer* self) {
    return (RDByteBuffer*)self;
}

static inline const RDByteBuffer* _rd_as_bytebuffer_c(const RDBuffer* self) {
    return (const RDByteBuffer*)self;
}

static bool _rd_bytebuffer_get_byte(const RDBuffer* self, usize idx, u8* b) {
    if(idx < self->length) {
        if(b) *b = _rd_as_bytebuffer_c(self)->data[idx];
        return true;
    }

    return false;
}

static bool _rd_bytebuffer_set_byte(RDBuffer* self, usize idx, u8 b) {
    if(idx < self->length) {
        _rd_as_bytebuffer(self)->data[idx] = b;
        return true;
    }

    return false;
}

static void _rd_bytebuffer_destroy(RDBuffer* self) {
    free(_rd_as_bytebuffer(self)->data);
    free(self);
}

RDByteBuffer* rd_i_buffer_create(usize n) {
    RDByteBuffer* self = calloc(1, sizeof(*self));
    self->base.get_byte = _rd_bytebuffer_get_byte;
    self->base.set_byte = _rd_bytebuffer_set_byte;
    self->base.destroy = _rd_bytebuffer_destroy;
    self->base.length = n;
    self->data = calloc(n, sizeof(*self->data));
    return self;
}

void rd_i_buffer_destroy(RDBuffer* self) {
    if(self) {
        vect_destroy(&self->str_buf);
        self->destroy(self);
    }
}

bool rd_i_buffer_is_empty(const RDBuffer* self) { return self->length == 0; }
usize rd_i_buffer_get_length(const RDBuffer* self) { return self->length; }

const char* rd_i_buffer_read_str(RDBuffer* self, usize idx, usize* len) {
    if(idx >= self->length) return NULL;

    vect_clear(&self->str_buf);
    vect_reserve(&self->str_buf, RD_STR_BUF_MIN_CAPACITY);
    usize i = idx;

    for(; i < self->length; i++) {
        u8 b;
        if(!self->get_byte(self, i, &b)) break;

        vect_push(&self->str_buf, (char)b);

        if(!b) {
            if(len) *len = i - idx;
            return self->str_buf.data;
        }
    }

    return NULL;
}

usize rd_i_buffer_read(const RDBuffer* self, usize idx, void* ptr, usize n) {
    if(!ptr) return 0;

    usize i = 0;

    for(; i < n; i++) {
        if(!self->get_byte(self, idx + i, (u8*)ptr + i)) break;
    }

    return i;
}

bool rd_i_buffer_read_u8(const RDBuffer* self, usize idx, u8* v) {
    return self->get_byte(self, idx, v);
}

bool rd_i_buffer_read_le16(const RDBuffer* self, usize idx, u16* v) {
    uint8_t b[2];
    if(!rd_i_buffer_read(self, idx, b, sizeof(b))) return false;

    if(v) *v = ((u16)b[0] << 0) | ((u16)b[1] << 8);
    return true;
}

bool rd_i_buffer_read_le32(const RDBuffer* self, usize idx, u32* v) {
    uint8_t b[4];
    if(!rd_i_buffer_read(self, idx, b, sizeof(b))) return false;

    if(v)
        *v = ((u32)b[0] << 0) | ((u32)b[1] << 8) | ((u32)b[2] << 16) |
             ((u32)b[3] << 24);

    return true;
}

bool rd_i_buffer_read_le64(const RDBuffer* self, usize idx, u64* v) {
    uint8_t b[8];
    if(!rd_i_buffer_read(self, idx, b, sizeof(b))) return false;

    if(v)
        *v = ((u64)b[0] << 0) | ((u64)b[1] << 8) | ((u64)b[2] << 16) |
             ((u64)b[3] << 24) | ((u64)b[4] << 32) | ((u64)b[5] << 40) |
             ((u64)b[6] << 48) | ((u64)b[7] << 56);

    return true;
}

bool rd_i_buffer_read_be16(const RDBuffer* self, usize idx, u16* v) {
    uint8_t b[2];
    if(!rd_i_buffer_read(self, idx, b, sizeof(b))) return false;

    if(v) *v = ((u16)b[0] << 8) | ((u16)b[1] << 0);
    return true;
}

bool rd_i_buffer_read_be32(const RDBuffer* self, usize idx, u32* v) {
    uint8_t b[4];
    if(!rd_i_buffer_read(self, idx, b, sizeof(b))) return false;

    if(v)
        *v = ((u32)b[0] << 24) | ((u32)b[1] << 16) | ((u32)b[2] << 8) |
             ((u32)b[3] << 0);

    return true;
}

bool rd_i_buffer_read_be64(const RDBuffer* self, usize idx, u64* v) {
    uint8_t b[8];
    if(!rd_i_buffer_read(self, idx, b, sizeof(b))) return false;

    if(v)
        *v = ((u64)b[0] << 56) | ((u64)b[1] << 48) | ((u64)b[2] << 40) |
             ((u64)b[3] << 32) | ((u64)b[4] << 24) | ((u64)b[5] << 16) |
             ((u64)b[6] << 8) | ((u64)b[7] << 0);

    return true;
}

bool rd_i_buffer_expect_u8(const RDBuffer* self, usize idx, u8 v) {
    u8 r;
    return rd_i_buffer_read_u8(self, idx, &r) && r == v;
}

bool rd_i_buffer_expect_le16(const RDBuffer* self, usize idx, u16 v) {
    u16 r;
    return rd_i_buffer_read_le16(self, idx, &r) && r == v;
}

bool rd_i_buffer_expect_le32(const RDBuffer* self, usize idx, u32 v) {
    u32 r;
    return rd_i_buffer_read_le32(self, idx, &r) && r == v;
}

bool rd_i_buffer_expect_le64(const RDBuffer* self, usize idx, u64 v) {
    u64 r;
    return rd_i_buffer_read_le64(self, idx, &r) && r == v;
}

bool rd_i_buffer_expect_be16(const RDBuffer* self, usize idx, u16 v) {
    u16 r;
    return rd_i_buffer_read_be16(self, idx, &r) && r == v;
}

bool rd_i_buffer_expect_be32(const RDBuffer* self, usize idx, u32 v) {
    u32 r;
    return rd_i_buffer_read_be32(self, idx, &r) && r == v;
}

bool rd_i_buffer_expect_be64(const RDBuffer* self, usize idx, u64 v) {
    u64 r;
    return rd_i_buffer_read_be64(self, idx, &r) && r == v;
}

bool rd_i_buffer_read_primitive(const RDBuffer* self, usize idx,
                                const char* name, bool big, u64* v) {
    if(!strcmp(name, "u8") || !strcmp(name, "i8") || !strcmp(name, "char")) {
        u8 val;
        if(!rd_i_buffer_read_u8(self, idx, &val)) return false;
        if(v) *v = val;
        return true;
    }

    if(!strcmp(name, "u16") || !strcmp(name, "i16") ||
       !strcmp(name, "char16")) {
        u16 val;
        bool ok = big ? rd_i_buffer_read_be16(self, idx, &val)
                      : rd_i_buffer_read_le16(self, idx, &val);
        if(!ok) return false;
        if(v) *v = val;
        return true;
    }

    if(!strcmp(name, "u32") || !strcmp(name, "i32")) {
        u32 val;
        bool ok = big ? rd_i_buffer_read_be32(self, idx, &val)
                      : rd_i_buffer_read_le32(self, idx, &val);
        if(!ok) return false;
        if(v) *v = val;
        return true;
    }

    if(!strcmp(name, "u64") || !strcmp(name, "i64")) {
        u64 val;
        bool ok = big ? rd_i_buffer_read_be64(self, idx, &val)
                      : rd_i_buffer_read_le64(self, idx, &val);
        if(!ok) return false;
        if(v) *v = val;
        return true;
    }

    return false;
}

bool rd_i_buffer_write(RDBuffer* self, usize idx, const void* ptr, usize n) {
    if(!ptr || idx >= self->length) return false;

    u8* p = (u8*)ptr;

    for(usize i = 0; i < n; i++) {
        if(!self->set_byte(self, idx + i, p[i])) return false;
    }

    return true;
}
