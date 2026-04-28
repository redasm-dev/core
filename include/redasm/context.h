#pragma once

#include <redasm/config.h>
#include <redasm/io/buffer.h>
#include <redasm/listing.h>
#include <redasm/mapping.h>
#include <redasm/plugins/analyzer.h>
#include <redasm/plugins/loader.h>
#include <redasm/plugins/plugin.h>
#include <redasm/plugins/processor/processor.h>
#include <redasm/segment.h>
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
    RD_DR_READ = 1,
    RD_DR_WRITE,
    RD_DR_ADDRESS,

    RD_CR_CALL,
    RD_CR_JUMP,
} RDRefType;

typedef struct RDProblem {
    RDAddress from_address;
    RDAddress address;
    const char* message;
} RDProblem;

typedef struct RDImported {
    const char* module;

    struct {
        u64 value;
        bool has_value;
    } ordinal;
} RDImported;

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

typedef struct RDSymbolSlice {
    const RDSymbol* data;
    usize length;
} RDSymbolSlice;

typedef struct RDXRef {
    RDAddress address;
    usize type;
} RDXRef;

typedef struct RDXRefSlice {
    const RDXRef* data;
    usize length;
} RDXRefSlice;

typedef struct RDDelaySlotInfo {
    RDInstruction instr;
    u8 n;
} RDDelaySlotInfo;

typedef struct RDLoadAddressing {
    RDAddress entrypoint;
    RDAddress address;
    RDOffset offset;
} RDLoadAddressing;

RD_API void rd_destroy(RDContext* self);
RD_API bool rd_step(RDContext* self, const RDWorkerStatus** status);
RD_API void rd_disassemble(RDContext* self);
RD_API bool rd_decode(RDContext* ctx, RDAddress address, RDInstruction* instr);
RD_API bool rd_decode_prev(RDContext* ctx, RDAddress address,
                           RDInstruction* instr);
RD_API RDProblemSlice rd_get_all_problems(const RDContext* self);
RD_API RDFunctionSlice rd_get_all_functions(const RDContext* self);
RD_API RDAddressSlice rd_get_all_exported(const RDContext* self);
RD_API RDAddressSlice rd_get_all_imported(const RDContext* self);
RD_API RDSymbolSlice rd_get_all_symbols(const RDContext* self);
RD_API RDSegmentSlice rd_get_all_segments(const RDContext* self);
RD_API RDInputMappingSlice rd_get_all_mappings(const RDContext* self);
RD_API RDLoadAddressing rd_get_load_addressing(const RDContext* self);
RD_API unsigned int rd_get_min_string(const RDContext* self);
RD_API void rd_set_min_string(RDContext* self, unsigned int l);
RD_API const RDSegment* rd_find_segment(const RDContext* self, RDAddress addr);
RD_API const RDFunction* rd_find_function(const RDContext* self,
                                          RDAddress address);
RD_API RDLoader* rd_get_loader(const RDContext* self);
RD_API RDProcessor* rd_get_processor(const RDContext* self);
RD_API const RDLoaderPlugin* rd_get_loader_plugin(const RDContext* self);
RD_API const RDProcessorPlugin* rd_get_processor_plugin(const RDContext* self);
RD_API RDAnalyzerItemSlice rd_get_analyzer_plugins(const RDContext* self);
RD_API bool rd_is_address(const RDContext* self, RDAddress address);
RD_API const char* rd_get_comment(RDContext* self, RDAddress address);
RD_API const char* rd_get_name(RDContext* self, RDAddress address);
RD_API const char* rd_to_hex(const RDContext* self, usize v);
RD_API const char* rd_render_text(RDContext* self, RDAddress address);
RD_API bool rd_undefine(RDContext* self, RDAddress address);
RD_API RDDelaySlotInfo rd_get_delay_slot_info(const RDContext* self);
RD_API bool rd_set_noreturn(RDContext* self, RDAddress address);
RD_API bool rd_set_comment(RDContext* self, RDAddress address, const char* cmt);
RD_API bool rd_auto_name(RDContext* self, RDAddress address, const char* name);
RD_API bool rd_library_name(RDContext* self, RDAddress address,
                            const char* name);
RD_API bool rd_user_name(RDContext* self, RDAddress address, const char* name);
RD_API bool rd_auto_function(RDContext* self, RDAddress address,
                             const char* name);
RD_API bool rd_library_function(RDContext* self, RDAddress address,
                                const char* name);
RD_API bool rd_user_function(RDContext* self, RDAddress address,
                             const char* name);
RD_API bool rd_get_entry_point(const RDContext* self, RDAddress* address);
RD_API bool rd_set_entry_point(RDContext* self, RDAddress address,
                               const char* name);
RD_API bool rd_set_exported(RDContext* self, RDAddress address,
                            const char* name);
RD_API bool rd_set_imported(RDContext* self, RDAddress address,
                            const char* module, const char* name);
RD_API bool rd_set_imported_ord(RDContext* self, RDAddress address,
                                const char* module, const char* name, u64 ord);
RD_API bool rd_get_imported(RDContext* self, RDAddress address,
                            RDImported* imp);
RD_API void rd_flow(RDContext* self, RDAddress addr);
RD_API bool rd_get_address(RDContext* self, const char* name,
                           RDAddress* address);
RD_API bool rd_to_offset(const RDContext* self, RDAddress address,
                         RDOffset* offset);
RD_API bool rd_to_address(const RDContext* self, RDOffset offset,
                          RDAddress* address);
RD_API bool rd_map_segment(RDContext* self, const char* name, RDAddress addr,
                           RDAddress endaddr, u32 perm);
RD_API bool rd_map_segment_n(RDContext* self, const char* name, RDAddress addr,
                             usize n, u32 perm);
RD_API bool rd_map_input(RDContext* self, RDOffset off, RDAddress addr,
                         RDAddress endaddr);
RD_API bool rd_map_input_n(RDContext* self, RDOffset off, RDAddress addr,
                           usize n);
RD_API bool rd_add_xref(RDContext* self, RDAddress fromaddr, RDAddress toaddr,
                        usize type);

RD_API RDXRefSlice rd_get_xrefs_from(RDContext* self, RDAddress fromaddr);
RD_API RDXRefSlice rd_get_xrefs_from_type(RDContext* self, RDAddress fromaddr,
                                          usize type);
RD_API RDXRefSlice rd_get_xrefs_to(RDContext* self, RDAddress toaddr);
RD_API RDXRefSlice rd_get_xrefs_to_type(RDContext* self, RDAddress toaddr,
                                        usize type);

RD_API RDReader* rd_get_reader(const RDContext* self);
RD_API RDReader* rd_get_input_reader(const RDContext* self);
RD_API bool rd_read_u8(const RDContext* self, RDAddress address, u8* v);
RD_API bool rd_read_le16(const RDContext* self, RDAddress address, u16* v);
RD_API bool rd_read_le32(const RDContext* self, RDAddress address, u32* v);
RD_API bool rd_read_le64(const RDContext* self, RDAddress address, u64* v);
RD_API bool rd_read_be16(const RDContext* self, RDAddress address, u16* v);
RD_API bool rd_read_be32(const RDContext* self, RDAddress address, u32* v);
RD_API bool rd_read_be64(const RDContext* self, RDAddress address, u64* v);
RD_API bool rd_expect_u8(const RDContext* self, RDAddress address, u8 v);
RD_API bool rd_expect_le16(const RDContext* self, RDAddress address, u16 v);
RD_API bool rd_expect_le32(const RDContext* self, RDAddress address, u32 v);
RD_API bool rd_expect_le64(const RDContext* self, RDAddress address, u64 v);
RD_API bool rd_expect_be16(const RDContext* self, RDAddress address, u16 v);
RD_API bool rd_expect_be32(const RDContext* self, RDAddress address, u32 v);
RD_API bool rd_expect_be64(const RDContext* self, RDAddress address, u64 v);

RD_API usize rd_read(const RDContext* self, RDAddress address, void* data,
                     usize n);
