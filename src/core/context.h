#pragma once

#include "core/callconv.h"
#include "core/engine.h"
#include "core/segment.h"
#include "core/symbol.h"
#include "db/db.h"
#include "hooks.h"
#include "io/buffer.h"
#include "kb/kb.h"
#include "plugins/analyzer.h"
#include "plugins/processor/processor.h"
#include "support/error.h"
#include "support/stringpool.h"
#include "support/utils.h"
#include <redasm/redasm.h>
#include <redasm/types/def.h>
#include <time.h>

typedef struct RDHooks RDHooks;

typedef struct RDDelaySlotInfo {
    RDInstruction instr;
    u8 n;
} RDDelaySlotInfo;

typedef struct RDPendingRename {
    RDAddress address;
    RDName name;
} RDPendingRename;

typedef struct RDPendingRenameVect {
    RDPendingRename* data;
    usize length;
    usize capacity;
} RDPendingRenameVect;

typedef struct RDStringTerminatorVect {
    u8* data;
    usize length;
    usize capacity;
} RDStringTerminatorVect;

typedef struct RDContext {
    const RDTestResult* testresult; // live only during loaderplugin->load()
    const RDLoaderPlugin* loaderplugin;
    const RDProcessorPlugin* processorplugin;
    RDAnalyzerItemVect analyzerplugins;

    RDProcessor* processor;

    RDStringPool strings;
    RDXRefVect xrefs_from;
    RDXRefVect xrefs_to;
    RDXRefVect und_xrefs;

    RDCharVect autoname_buf;
    RDCharVect nameaddr_buf;
    RDCharVect name_buf;
    RDCharVect str_buf;
    RDCharVect sym_buf;
    RDCharVect imp_buf;
    RDCharVect problem_buf;
    RDCharVect tdef_buf;
    RDCharVect type_buf;
    RDCharVect seg_buf;
    RDAddressVect addr_type_buf;
    RDFunctionChunkVect chunk_buf;
    RDOvrOperandVect ovr_ops_buf;
    RDInstructionVect lift_buf;
    RDResolveResultVect resolve_buf;
    RDProblemsVect problems_buf;
    RDSymbolVect symbols_buf;

    u32 func_gen;
    u32 graph_gen;

    char* working_dir;
    char* file_name;

    RDByteBuffer* input;
    RDReader* input_reader;
    RDReader* reader;

    bool scan_char16;
    int min_string;
    RDAddress base_address;
    RDLoadAddressing addressing;

    RDDB* db;
    RDKB* kb;
    RDFunctionVect functions;
    RDExternalVect externals;
    RDHookItemVect hooks;

    RDCallConvVect callconvs;
    RDTypeDefVect typedefs;
    RDPendingRenameVect pending_renames;
    RDStringTerminatorVect string_terminators;

    struct {
        RDRelAddress value;
        bool has_value;
    } entry_point;

    struct {
        const RDSegment* segment;
        RDDelaySlotInfo dslot_info;
        RDEngineQueue qdirty;
        RDEngineQueue qjump;
        RDEngineQueue qcall;
        RDEngineItem current;
        RDEngineFlow flow;
        unsigned int step;
        clock_t emulate_start;
    } engine;
} RDContext;

static inline RDAddress rd_i_abs(const RDContext* ctx, RDRelAddress rel) {
    RDAddress result = (RDAddress)(rel + ctx->base_address);
    panic_if(result < rel, "relative address %llX overflows", rel);
    return result;
}

static inline RDRelAddress rd_i_rel(const RDContext* ctx, RDAddress abs) {
    panic_if(abs < ctx->base_address,
             "absolute address %llX underflows base address %llX", abs,
             ctx->base_address);
    return abs - ctx->base_address;
}

static inline bool rd_i_segment_contains(const RDSegment* seg, RDAddress addr) {
    return addr >= rd_segment_get_start(seg) && addr < rd_segment_get_end(seg);
}

void rd_i_expand_range(RDContext* self, const RDSegment* seg, usize* start,
                       usize* end);

RDContext* rd_i_context_create(const RDLoaderPlugin* lplugin,
                               RDByteBuffer* input, const char* workingdir,
                               const char* filename, const char* dbpath);

void rd_i_set_processor(RDContext* self, const RDProcessorPlugin* plugin);

bool rd_i_get_name(RDContext* self, RDAddress address, bool autoname,
                   RDName* n);
bool rd_i_get_name_to(RDContext* self, RDAddress address, bool autoname,
                      RDName* n, RDCharVect* buf);
bool rd_i_set_name(RDContext* self, RDAddress address, const char* name,
                   RDConfidence c);
bool rd_i_add_xref(RDContext* self, RDAddress fromaddr, RDAddress toaddr,
                   RDXRefType type, RDConfidence c);
bool rd_i_del_xref(RDContext* self, RDAddress fromaddr, RDAddress toaddr,
                   RDConfidence c);

bool rd_i_set_external(RDContext* self, const RDExternal* ext);

bool rd_i_undefine(RDContext* self, RDAddress address, RDConfidence c);
bool rd_i_undefine_n(RDContext* self, RDAddress address, usize n,
                     RDConfidence c);
void rd_i_clear_n(RDContext* self, RDAddress address, usize n);

bool rd_i_add_comment(RDContext* self, RDAddress address, const char* cmt,
                      RDCommentPlacement p);
bool rd_i_del_comment(RDContext* self, RDAddress address, RDCommentPlacement p);

const RDXRefVect* rd_i_get_xrefs_from(RDContext* self, RDAddress fromaddr,
                                      RDXRefType type);
const RDXRefVect* rd_i_get_xrefs_to(RDContext* self, RDAddress toaddr,
                                    RDXRefType type);
const RDXRefVect* rd_i_get_xrefs_from_ex(RDContext* self, RDAddress fromaddr,
                                         RDXRefType type, RDXRefVect* r);
const RDXRefVect* rd_i_get_xrefs_to_ex(RDContext* self, RDAddress toaddr,
                                       RDXRefType type, RDXRefVect* r);

RDFunction* rd_i_find_function(const RDContext* self, RDAddress address);
RDFunction* rd_i_get_function(const RDContext* self, RDAddress address);

bool rd_i_set_noret(RDContext* self, RDAddress address);

void rd_i_add_problem(RDContext* self, RDAddress from, RDAddress address,
                      const char* fmt, ...);
