#pragma once

#include "support/utils.h"
#include <redasm/io/buffer.h>

typedef struct RDBuffer {
    usize length;
    RDCharVect str_buf;
    bool (*get_byte)(const struct RDBuffer* self, usize idx, u8* b);
    bool (*set_byte)(struct RDBuffer* self, usize idx, u8 b);
    void (*destroy)(struct RDBuffer* self);
} RDBuffer;

typedef struct RDByteBuffer {
    RDBuffer base;
    u8* data;
} RDByteBuffer;

RDByteBuffer* rd_i_buffer_create(usize n);
void rd_i_buffer_destroy(RDBuffer* self);
bool rd_i_buffer_write(RDBuffer* self, usize idx, const void* ptr, usize n);
bool rd_i_buffer_is_empty(const RDBuffer* self);
usize rd_i_buffer_get_length(const RDBuffer* self);
bool rd_i_buffer_read_u8(const RDBuffer* self, usize idx, u8* v);
bool rd_i_buffer_read_le16(const RDBuffer* self, usize idx, u16* v);
bool rd_i_buffer_read_le32(const RDBuffer* self, usize idx, u32* v);
bool rd_i_buffer_read_le64(const RDBuffer* self, usize idx, u64* v);
bool rd_i_buffer_read_be16(const RDBuffer* self, usize idx, u16* v);
bool rd_i_buffer_read_be32(const RDBuffer* self, usize idx, u32* v);
bool rd_i_buffer_read_be64(const RDBuffer* self, usize idx, u64* v);
bool rd_i_buffer_expect_u8(const RDBuffer* self, usize idx, u8 v);
bool rd_i_buffer_expect_le16(const RDBuffer* self, usize idx, u16 v);
bool rd_i_buffer_expect_le32(const RDBuffer* self, usize idx, u32 v);
bool rd_i_buffer_expect_le64(const RDBuffer* self, usize idx, u64 v);
bool rd_i_buffer_expect_be16(const RDBuffer* self, usize idx, u16 v);
bool rd_i_buffer_expect_be32(const RDBuffer* self, usize idx, u32 v);
bool rd_i_buffer_expect_be64(const RDBuffer* self, usize idx, u64 v);
const char* rd_i_buffer_read_str(RDBuffer* self, usize idx, usize* len);
usize rd_i_buffer_read(const RDBuffer* self, usize idx, void* ptr, usize n);

bool rd_i_buffer_read_primitive(const RDBuffer* self, usize idx,
                                const char* name, bool big, u64* v);
