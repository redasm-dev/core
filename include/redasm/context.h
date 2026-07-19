#pragma once

#include <redasm/config.h>
#include <redasm/io/buffer.h>
#include <redasm/mapping.h>
#include <redasm/plugins/analyzer.h>
#include <redasm/plugins/loader.h>
#include <redasm/plugins/plugin.h>
#include <redasm/plugins/processor/processor.h>
#include <redasm/segment.h>
#include <redasm/symbol.h>
#include <redasm/version.h>

typedef enum {
    RD_SP_NONE = 0,
    RD_SP_R = 1 << 0,
    RD_SP_W = 1 << 1,
    RD_SP_X = 1 << 2,

    RD_SP_RW = RD_SP_R | RD_SP_W,
    RD_SP_RX = RD_SP_R | RD_SP_X,
    RD_SP_RWX = RD_SP_R | RD_SP_W | RD_SP_X,
    RD_SP_WX = RD_SP_W | RD_SP_X,
} RDSegmentPerm;

typedef enum {
    RD_XR_NONE = 0,
    RD_DR_READ,
    RD_DR_WRITE,
    RD_DR_ADDRESS,

    RD_CR_CALL,
    RD_CR_JUMP,
} RDXRefType;

typedef enum {
    RD_EXT_NONE = 0,
    RD_EXT_IMPORTED,
    RD_EXT_EXPORTED,
} RDExternalKind;

typedef enum {
    RD_EXPORT_DB = 0,
} RDExportFormat;

typedef struct RDProblem {
    RDAddress from_address;
    RDAddress address;
    const char* message;
} RDProblem;

typedef struct RDExternal {
    RDExternalKind kind;
    RDAddress address;
    const char* module;

    struct {
        u32 value;
        bool has_value;
    } ordinal;
} RDExternal;

typedef struct RDProblemSlice {
    const RDProblem* data;
    usize length;
} RDProblemSlice;

typedef struct RDWorkerStatus {
    bool is_listing_changed;
    bool is_busy;
    const char* step;
    const RDSegment* segment;
    usize pending_calls;
    usize pending_jumps;

    struct {
        RDAddress value;
        bool has_value;
    } address;
} RDWorkerStatus;

typedef struct RDAddressSlice {
    const RDAddress* data;
    usize length;
} RDAddressSlice;

typedef struct RDExternalSlice {
    const RDExternal* data;
    usize length;
} RDExternalSlice;

typedef struct RDXRef {
    RDAddress address;
    RDXRefType type;
} RDXRef;

typedef struct RDXRefSlice {
    const RDXRef* data;
    usize length;
} RDXRefSlice;

typedef struct RDLoadAddressing {
    RDAddress entrypoint;
    RDAddress address;
    RDOffset offset;
} RDLoadAddressing;

typedef struct RDAddressSpace {
    RDAddress start;
    RDAddress end;
    usize size;
} RDAddressSpace;

// clang-format off
RD_API void rd_destroy(RDContext* self);
RD_API bool rd_step(RDContext* self, RDWorkerStatus* status);
RD_API bool rd_export(RDContext* self, const char* filepath, RDExportFormat f);
RD_API bool rd_project_save(RDContext* self, const char* filepath);
RD_API void rd_disassemble(RDContext* self);
RD_API void rd_set_scan_char16(RDContext* self, bool b);
RD_API const char* rd_get_working_dir(const RDContext* self);
RD_API const char* rd_get_file_name(const RDContext* self);
RD_API const char* rd_str_intern(RDContext* self, const char* s);
RD_API const char* rd_resolve_name(RDContext* self, RDAddress address, RDAddress* resolved);
RD_API bool rd_patch_instruction(RDContext* self, RDAddress address, const char* instr, bool fill_nops);
RD_API bool rd_encode(RDContext* ctx, RDAddress address, const char* s, RDScratchBuffer* buf);
RD_API bool rd_decode(RDContext* ctx, RDAddress address, RDInstruction* instr);
RD_API bool rd_decode_n(RDContext* ctx, RDAddress address, RDInstruction* instrs, usize n);
RD_API bool rd_decode_prev(RDContext* ctx, RDAddress address, RDInstruction* instr);
RD_API RDProblemSlice rd_get_all_problems(const RDContext* self);
RD_API RDAddressSlice rd_get_all_address_by_type(const RDContext* self, const char* filter);
RD_API RDFunctionSlice rd_get_all_functions(const RDContext* self);
RD_API RDExternalSlice rd_get_all_externals(const RDContext* self, RDExternalKind kind);
RD_API RDSymbolSlice rd_get_all_symbols(const RDContext* self);
RD_API RDSegmentSlice rd_get_all_segments(const RDContext* self);
RD_API RDAddressSpace rd_get_address_space(const RDContext* ctx);
RD_API RDInputMappingSlice rd_get_all_mappings(const RDContext* self);
RD_API RDLoadAddressing rd_get_load_addressing(const RDContext* self);
RD_API void rd_set_min_string(RDContext* self, int l);
RD_API const RDSegment* rd_find_segment(const RDContext* self, RDAddress addr);
RD_API const RDFunction* rd_find_function(const RDContext* self, RDAddress address);
RD_API const RDLoaderPlugin* rd_get_loader_plugin(const RDContext* self);
RD_API const RDProcessorPlugin* rd_get_processor_plugin(const RDContext* self);
RD_API RDAnalyzerItemSlice rd_get_analyzer_plugins(const RDContext* self);
RD_API bool rd_is_address(const RDContext* self, RDAddress address);
RD_API const char* rd_get_comment(RDContext* self, RDAddress address);
RD_API const char* rd_get_name(RDContext* self, RDAddress address);
RD_API const char* rd_to_dec(i64 v);
RD_API const char* rd_to_hex(i64 v);
RD_API const char* rd_to_hexaddr(const RDContext* self, usize v);
RD_API const char* rd_render_text(RDContext* self, RDAddress address);
RD_API bool rd_auto_undefine(RDContext* self, RDAddress address);
RD_API bool rd_library_undefine(RDContext* self, RDAddress address);
RD_API bool rd_user_undefine(RDContext* self, RDAddress address);
RD_API bool rd_auto_undefine_n(RDContext* self, RDAddress address, usize n);
RD_API bool rd_library_undefine_n(RDContext* self, RDAddress address, usize n);
RD_API bool rd_user_undefine_n(RDContext* self, RDAddress address, usize n);
RD_API bool rd_set_noreturn(RDContext* self, RDAddress address);
RD_API bool rd_set_comment(RDContext* self, RDAddress address, const char* cmt);
RD_API bool rd_placeholder_name(RDContext* self, RDAddress address, const char* name);
RD_API bool rd_auto_name(RDContext* self, RDAddress address, const char* name);
RD_API bool rd_library_name(RDContext* self, RDAddress address, const char* name);
RD_API bool rd_user_name(RDContext* self, RDAddress address, const char* name);
RD_API bool rd_placeholder_function(RDContext* self, RDAddress address, const char* name);
RD_API bool rd_auto_function(RDContext* self, RDAddress address, const char* name);
RD_API bool rd_library_function(RDContext* self, RDAddress address, const char* name);
RD_API bool rd_user_function(RDContext* self, RDAddress address, const char* name);
RD_API bool rd_get_entry_point(const RDContext* self, RDAddress* address);
RD_API bool rd_set_entry_point(RDContext* self, RDAddress address, const char* name);
RD_API bool rd_get_external(RDContext* self, RDAddress address, RDExternal* ext);
RD_API bool rd_get_external_ord(RDContext* self, const char* module, u32 ord, RDExternalKind kind, RDExternal* ext);
RD_API bool rd_set_external(RDContext* self, RDAddress address, const char* module, RDExternalKind kind);
RD_API bool rd_set_external_ord(RDContext* self, RDAddress address, const char* module, u32 ord, RDExternalKind kind);
RD_API void rd_flow(RDContext* self, RDAddress addr);
RD_API bool rd_has_refs_from(const RDContext* self, RDAddress address);
RD_API bool rd_has_refs_to(const RDContext* self, RDAddress address);
RD_API bool rd_get_address(RDContext* self, const char* name, RDAddress* address);
RD_API bool rd_to_offset(const RDContext* self, RDAddress address, RDOffset* offset);
RD_API bool rd_to_address(const RDContext* self, RDOffset offset, RDAddress* address);
RD_API bool rd_map_segment(RDContext* self, const char* name, RDAddress addr, RDAddress endaddr, u32 perm);
RD_API bool rd_map_segment_n(RDContext* self, const char* name, RDAddress addr, usize n, u32 perm);
RD_API bool rd_map_input(RDContext* self, RDOffset off, RDAddress addr, RDAddress endaddr);
RD_API bool rd_map_input_n(RDContext* self, RDOffset off, RDAddress addr, usize n);
RD_API bool rd_add_xref(RDContext* self, RDAddress fromaddr, RDAddress toaddr, RDXRefType type);
RD_API RDXRefSlice rd_get_xrefs_from(RDContext* self, RDAddress fromaddr, RDXRefType type);
RD_API RDXRefSlice rd_get_xrefs_to(RDContext* self, RDAddress toaddr, RDXRefType type);
RD_API bool rd_operand_as_address(RDContext* self, RDAddress address, int index);
RD_API bool rd_operand_as_immediate(RDContext* self, RDAddress address, int index);
RD_API RDReader* rd_get_reader(const RDContext* self);
RD_API RDReader* rd_get_input_reader(const RDContext* self);
RD_API bool rd_read_byte(const RDContext* self, RDAddress address, u8* v);
RD_API bool rd_read_le16(const RDContext* self, RDAddress address, u16* v);
RD_API bool rd_read_le32(const RDContext* self, RDAddress address, u32* v);
RD_API bool rd_read_le64(const RDContext* self, RDAddress address, u64* v);
RD_API bool rd_read_be16(const RDContext* self, RDAddress address, u16* v);
RD_API bool rd_read_be32(const RDContext* self, RDAddress address, u32* v);
RD_API bool rd_read_be64(const RDContext* self, RDAddress address, u64* v);
RD_API const char* rd_read_str(const RDContext* self, RDAddress address, usize* n);
RD_API usize rd_read(const RDContext* self, RDAddress address, void* data, usize n);
RD_API bool rd_expect_u8(const RDContext* self, RDAddress address, u8 v);
RD_API bool rd_expect_le16(const RDContext* self, RDAddress address, u16 v);
RD_API bool rd_expect_le32(const RDContext* self, RDAddress address, u32 v);
RD_API bool rd_expect_le64(const RDContext* self, RDAddress address, u64 v);
RD_API bool rd_expect_be16(const RDContext* self, RDAddress address, u16 v);
RD_API bool rd_expect_be32(const RDContext* self, RDAddress address, u32 v);
RD_API bool rd_expect_be64(const RDContext* self, RDAddress address, u64 v);
RD_API bool rd_read_ptr(const RDContext* ctx, RDAddress address, RDAddress* v);
RD_API bool rd_follow_ptr(RDContext* ctx, RDAddress address, RDAddress* v);
RD_API bool rd_write_byte(RDContext* self, RDAddress address, u8 v);
RD_API bool rd_write_le16(RDContext* self, RDAddress address, u16 v);
RD_API bool rd_write_le32(RDContext* self, RDAddress address, u32 v);
RD_API bool rd_write_le64(RDContext* self, RDAddress address, u64 v);
RD_API bool rd_write_be16(RDContext* self, RDAddress address, u16 v);
RD_API bool rd_write_be32(RDContext* self, RDAddress address, u32 v);
RD_API bool rd_write_be64(RDContext* self, RDAddress address, u64 v);
RD_API usize rd_write(RDContext* self, RDAddress address, const void* data, usize n);
RD_API bool rd_fill(RDContext* self, RDAddress address, usize n);
// clang-format on
