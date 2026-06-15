#pragma once

#include "core/engine.h"
#include "core/segment.h"
#include "db/db.h"
#include "io/buffer.h"
#include "kb/kb.h"
#include "listing/listing.h"
#include "plugins/processor/processor.h"
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

typedef struct RDContext {
    const RDLoaderPlugin* loaderplugin;
    const RDProcessorPlugin* processorplugin;
    RDLoader* loader;
    RDProcessor* processor;

    struct {
        RDAnalyzerItem** data;
        usize length;
        usize capacity;
    } analyzerplugins;

    RDStringPool strings;
    RDXRefVect xrefs_from;
    RDXRefVect xrefs_to;
    RDXRefVect und_xrefs;

    RDCharVect name_buf;
    RDCharVect str_buf;
    RDCharVect sym_buf;
    RDCharVect imp_buf;
    RDCharVect problem_buf;
    RDCharVect tdef_buf;
    RDCharVect type_buf;
    RDCharVect imp_hint_buf;
    RDCharVect seg_buf;
    RDFunctionChunkVect chunk_buf;
    RDOvrOperandVect ovr_ops_buf;
    RDInstructionVect lift_buf;

    char* loader_name;
    char* filepath;
    RDByteBuffer* input;
    RDReader* input_reader;
    RDReader* reader;

    bool scan_char16;
    int min_string;
    RDLoadAddressing addressing;

    RDDB* db;
    RDKB* kb;
    RDListing listing;
    RDFunctionVect functions;
    RDAddressVect exported;
    RDAddressVect imported;
    RDHooks* hooks;

    RDTypeDefVect types[RD_TKIND_COUNT];

    struct {
        RDAddress value;
        bool has_value;
    } entry_point;

    struct {
        const RDSegmentFull* segment;
        RDDelaySlotInfo dslot_info;
        RDEngineQueue qjump;
        RDEngineQueue qcall;
        RDEngineItem current;
        RDEngineFlow flow;
        unsigned int step;
        clock_t emulate_start;
    } engine;

    struct {
        RDProblem* data;
        usize length;
        usize capacity;
    } problems;
} RDContext;

static inline bool rd_i_segment_contains(const RDSegmentFull* seg,
                                         RDAddress addr) {
    return addr >= seg->base.start_address && addr < seg->base.end_address;
}

RDContext* rd_i_context_create(const RDLoaderPlugin* lplugin, RDLoader* ldr,
                               const RDProcessorPlugin* pplugin,
                               RDByteBuffer* input, const char* filepath,
                               const char* dbpath);
bool rd_i_get_name(RDContext* self, RDAddress address, bool autoname,
                   RDName* n);
bool rd_i_set_name(RDContext* self, RDAddress address, const char* name,
                   RDConfidence c);
bool rd_i_set_function(RDContext* self, RDAddress address, const char* name,
                       RDConfidence c);
bool rd_i_set_imported(RDContext* self, RDAddress address, const char* name,
                       const RDImported* imp);

bool rd_i_add_xref(RDContext* self, RDAddress fromaddr, RDAddress toaddr,
                   RDXRefType type, RDConfidence c);
bool rd_i_del_xref(RDContext* self, RDAddress fromaddr, RDAddress toaddr,
                   RDConfidence c);

bool rd_i_undefine(RDContext* self, RDAddress address, RDConfidence c);
bool rd_i_undefine_n(RDContext* self, RDAddress address, usize n,
                     RDConfidence c);

const RDXRefVect* rd_i_get_xrefs_from(RDContext* self, RDAddress fromaddr,
                                      RDXRefType type);
const RDXRefVect* rd_i_get_xrefs_to(RDContext* self, RDAddress toaddr,
                                    RDXRefType type);
const RDXRefVect* rd_i_get_xrefs_from_ex(RDContext* self, RDAddress fromaddr,
                                         RDXRefType type, RDXRefVect* r);
const RDXRefVect* rd_i_get_xrefs_to_ex(RDContext* self, RDAddress toaddr,
                                       RDXRefType type, RDXRefVect* r);

RDFunction* rd_i_find_function(const RDContext* self, RDAddress address);

bool rd_i_set_noret(RDContext* self, RDAddress address);

const char* rd_i_get_imported_hint(RDContext* ctx, const char* name,
                                   RDCharVect* v);
const char* rd_i_get_imported_ord_hint(RDContext* ctx, const char* module,
                                       u32 ordinal, RDCharVect* v);

void rd_i_add_problem(RDContext* self, RDAddress from, RDAddress address,
                      const char* fmt, ...);
