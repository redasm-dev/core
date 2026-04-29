#include "reader.h"
#include "core/context.h"
#include "core/segment.h"
#include <stdlib.h>

typedef struct RDFlagsReader {
    RDReader base;
    const RDContext* context;
} RDFlagsReader;

static void _rd_reader_seek(RDReader* self, u64 pos) {
    if(pos > self->buffer->length) pos = self->buffer->length;
    self->position = pos;
}

static u64 _rd_reader_get_pos(const RDReader* self) { return self->position; }

static void _rd_flagsreader_seek(RDReader* self, usize pos) {
    RDFlagsReader* fr = (RDFlagsReader*)self;
    self->segment = rd_i_find_segment(fr->context, pos);

    if(self->segment) {
        fr->base.position = rd_i_address2index(self->segment, pos);
        fr->base.buffer = (RDBuffer*)self->segment->flags;
        fr->base.error = false;
    }
    else {
        fr->base.buffer = NULL;
        fr->base.error = true;
    }
}

static u64 _rd_flagsreader_get_pos(const RDReader* self) {
    return rd_i_index2address(self->segment, self->position);
}

RDReader* rd_i_reader_create(RDBuffer* buf) {
    RDReader* self = malloc(sizeof(*self));

    *self = (RDReader){
        .buffer = buf,
        .seek = _rd_reader_seek,
        .get_pos = _rd_reader_get_pos,
    };

    return self;
}

RDReader* rd_i_reader_create_flags(RDContext* ctx) {
    RDFlagsReader* self = malloc(sizeof(*self));

    *self = (RDFlagsReader){
        .context = ctx,
        .base =
            {
                .seek = _rd_flagsreader_seek,
                .get_pos = _rd_flagsreader_get_pos,
            },
    };

    return (RDReader*)self;
}

void rd_i_reader_destroy(RDReader* self) { free(self); }
void rd_reader_seek(RDReader* self, u64 pos) { self->seek(self, pos); }
usize rd_reader_get_pos(const RDReader* self) { return self->get_pos(self); }
u64 rd_reader_get_length(const RDReader* self) { return self->buffer->length; }
bool rd_reader_has_error(const RDReader* self) { return self->error; }

bool rd_reader_at_end(const RDReader* self) {
    return self->error || self->position >= self->buffer->length;
}

bool rd_reader_read(RDReader* self, void* v, usize n) {
    if(!self->error && self->position < self->buffer->length &&
       rd_i_buffer_read(self->buffer, self->position, v, n)) {
        self->position += n;
        return true;
    }

    self->error = true;
    return false;
}

bool rd_reader_read_u8(RDReader* self, u8* v) {
    if(self->error || self->position >= self->buffer->length) return false;

    self->error = !rd_i_buffer_read_u8(self->buffer, self->position, v);
    if(!self->error) self->position += sizeof(u8);
    return !self->error;
}

bool rd_reader_read_le16(RDReader* self, u16* v) {
    if(self->error || self->position >= self->buffer->length) return false;

    self->error = !rd_i_buffer_read_le16(self->buffer, self->position, v);
    if(!self->error) self->position += sizeof(u16);
    return !self->error;
}

bool rd_reader_read_le32(RDReader* self, u32* v) {
    if(self->error || self->position >= self->buffer->length) return false;

    self->error = !rd_i_buffer_read_le32(self->buffer, self->position, v);
    if(!self->error) self->position += sizeof(u32);
    return !self->error;
}

bool rd_reader_read_le64(RDReader* self, u64* v) {
    if(self->error || self->position >= self->buffer->length) return false;

    self->error = !rd_i_buffer_read_le64(self->buffer, self->position, v);
    if(!self->error) self->position += sizeof(u64);
    return !self->error;
}

bool rd_reader_read_be16(RDReader* self, u16* v) {
    if(self->error || self->position >= self->buffer->length) return false;

    self->error = !rd_i_buffer_read_be16(self->buffer, self->position, v);
    if(!self->error) self->position += sizeof(u16);
    return !self->error;
}

bool rd_reader_read_be32(RDReader* self, u32* v) {
    if(self->error || self->position >= self->buffer->length) return false;

    self->error = !rd_i_buffer_read_be32(self->buffer, self->position, v);
    if(!self->error) self->position += sizeof(u32);
    return !self->error;
}

bool rd_reader_read_be64(RDReader* self, u64* v) {
    if(self->error || self->position >= self->buffer->length) return false;

    self->error = !rd_i_buffer_read_be64(self->buffer, self->position, v);
    if(!self->error) self->position += sizeof(u64);
    return !self->error;
}

const char* rd_reader_read_str(RDReader* self, usize* len) {
    if(self->error || self->position >= self->buffer->length) return NULL;

    usize slen;
    const char* s = rd_i_buffer_read_str(self->buffer, self->position, &slen);

    if(s) {
        if(len) *len = slen;
        self->position += slen + 1;
    }
    else
        self->error = true;

    return s;
}

bool rd_reader_peek_u8(const RDReader* self, u8* v) {
    return !self->error && self->position < self->buffer->length &&
           rd_i_buffer_read_u8(self->buffer, self->position, v);
}

bool rd_reader_peek_le16(const RDReader* self, u16* v) {
    return !self->error && self->position < self->buffer->length &&
           rd_i_buffer_read_le16(self->buffer, self->position, v);
}

bool rd_reader_peek_le32(const RDReader* self, u32* v) {
    return !self->error && self->position < self->buffer->length &&
           rd_i_buffer_read_le32(self->buffer, self->position, v);
}

bool rd_reader_peek_le64(const RDReader* self, u64* v) {
    return !self->error && self->position < self->buffer->length &&
           rd_i_buffer_read_le64(self->buffer, self->position, v);
}

bool rd_reader_peek_be16(const RDReader* self, u16* v) {
    return !self->error && self->position < self->buffer->length &&
           rd_i_buffer_read_be16(self->buffer, self->position, v);
}

bool rd_reader_peek_be32(const RDReader* self, u32* v) {
    return !self->error && self->position < self->buffer->length &&
           rd_i_buffer_read_be32(self->buffer, self->position, v);
}

bool rd_reader_peek_be64(const RDReader* self, u64* v) {
    return !self->error && self->position < self->buffer->length &&
           rd_i_buffer_read_be64(self->buffer, self->position, v);
}

const char* rd_reader_peek_str(RDReader* self, usize* len) {
    if(self->error || self->position >= self->buffer->length) return NULL;
    return rd_i_buffer_read_str(self->buffer, self->position, len);
}

bool rd_reader_expect_u8(RDReader* self, u8 v) {
    u8 r;
    return rd_reader_read_u8(self, &r) && r == v;
}

bool rd_reader_expect_le16(RDReader* self, u16 v) {
    u16 r;
    return rd_reader_read_le16(self, &r) && r == v;
}

bool rd_reader_expect_le32(RDReader* self, u32 v) {
    u32 r;
    return rd_reader_read_le32(self, &r) && r == v;
}

bool rd_reader_expect_le64(RDReader* self, u64 v) {
    u64 r;
    return rd_reader_read_le64(self, &r) && r == v;
}

bool rd_reader_expect_be16(RDReader* self, u16 v) {
    u16 r;
    return rd_reader_read_be16(self, &r) && r == v;
}

bool rd_reader_expect_be32(RDReader* self, u32 v) {
    u32 r;
    return rd_reader_read_be32(self, &r) && r == v;
}

bool rd_reader_expect_be64(RDReader* self, u64 v) {
    u64 r;
    return rd_reader_read_be64(self, &r) && r == v;
}
