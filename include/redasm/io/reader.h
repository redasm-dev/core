#pragma once

#include <redasm/config.h>

typedef struct RDReader RDReader;

RD_API void rd_reader_seek(RDReader* self, u64 pos);
RD_API u64 rd_reader_get_pos(const RDReader* self);
RD_API u64 rd_reader_get_length(const RDReader* self);
RD_API bool rd_reader_has_error(const RDReader* self);
RD_API bool rd_reader_at_end(const RDReader* self);
RD_API bool rd_reader_read(RDReader* self, void* v, usize n);
RD_API bool rd_reader_read_u8(RDReader* self, u8* v);
RD_API bool rd_reader_read_le16(RDReader* self, u16* v);
RD_API bool rd_reader_read_le32(RDReader* self, u32* v);
RD_API bool rd_reader_read_le64(RDReader* self, u64* v);
RD_API bool rd_reader_read_be16(RDReader* self, u16* v);
RD_API bool rd_reader_read_be32(RDReader* self, u32* v);
RD_API bool rd_reader_read_be64(RDReader* self, u64* v);
RD_API const char* rd_reader_read_str(RDReader* self, usize* len);
RD_API bool rd_reader_peek_u8(const RDReader* self, u8* v);
RD_API bool rd_reader_peek_le16(const RDReader* self, u16* v);
RD_API bool rd_reader_peek_le32(const RDReader* self, u32* v);
RD_API bool rd_reader_peek_le64(const RDReader* self, u64* v);
RD_API bool rd_reader_peek_be16(const RDReader* self, u16* v);
RD_API bool rd_reader_peek_be32(const RDReader* self, u32* v);
RD_API bool rd_reader_peek_be64(const RDReader* self, u64* v);
RD_API const char* rd_reader_peek_str(RDReader* self, usize* len);
RD_API bool rd_reader_expect_u8(RDReader* self, u8 v);
RD_API bool rd_reader_expect_le16(RDReader* self, u16 v);
RD_API bool rd_reader_expect_le32(RDReader* self, u32 v);
RD_API bool rd_reader_expect_le64(RDReader* self, u64 v);
RD_API bool rd_reader_expect_be16(RDReader* self, u16 v);
RD_API bool rd_reader_expect_be32(RDReader* self, u32 v);
RD_API bool rd_reader_expect_be64(RDReader* self, u64 v);
