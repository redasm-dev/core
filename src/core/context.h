#pragma once

#include "core/db/db.h"
#include "core/segment.h"
#include "io/buffer.h"
#include "listing/listing.h"
#include "rdil/rdil.h"
#include "support/stringpool.h"
#include "support/utils.h"
#include <redasm/redasm.h>
#include <redasm/types/def.h>

typedef struct RDHooks RDHooks;

typedef enum {
    RD_WI_NONE = 0,
    RD_WI_FLOW,
    RD_WI_JUMP,
    RD_WI_CALL,
} RDWorkerItemKind;

typedef struct RDWorkerItem {
    RDWorkerItemKind kind;
    RDConfidence confidence;
    RDAddress address;
    char* name;
} RDWorkerItem;

typedef struct RDWorkerQueue {
    RDWorkerItem* data;
    usize length;
    usize capacity;
    usize head;
} RDWorkerQueue;

typedef struct RDAnalyzerItem {
    const RDAnalyzerPlugin* plugin;
    bool is_selected;
    usize n_runs;
} RDAnalyzerItem;

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
    RDXRefVect xrefs_from_type;
    RDXRefVect xrefs_to;
    RDXRefVect xrefs_to_type;

    RDCharVect name_buf;
    RDCharVect str_buf;
    RDCharVect sym_buf;
    RDCharVect imp_buf;
    RDCharVect problem_buf;
    RDTrackedRegVect tregs_buf;
    RDOvrOperandVect ovr_ops_buf;
    RDILInstruction il_buf;

    char* filepath;
    RDByteBuffer* input;
    RDReader* input_reader;
    RDReader* reader;

    unsigned int min_string;
    RDLoadAddressing addressing;

    RDDB db;
    RDListing listing;
    RDHooks* hooks;

    struct {
        RDTypeDef** data;
        usize length;
        usize capacity;
    } types;

    struct {
        RDSegmentFull** data;
        usize length;
        usize capacity;
    } segments;

    struct {
        RDInputMapping* data;
        usize length;
        usize capacity;
    } mappings;

    struct {
        RDAddress value;
        bool has_value;
    } entry_point;

    struct {
        const RDSegmentFull* segment;
        RDDelaySlotInfo dslot_info;
        RDWorkerQueue qjump;
        RDWorkerQueue qcall;
        RDWorkerItem flow;
        RDWorkerItem current;
        RDWorkerStatus status;
        unsigned int step;
    } engine;

    struct {
        RDProblem* data;
        usize length;
        usize capacity;
    } problems;
} RDContext;

const RDSegmentFull* rd_i_find_segment(const RDContext* self, RDAddress addr);

RDContext* rd_i_context_create(const RDLoaderPlugin* lplugin, RDLoader* ldr,
                               const char* filepath, RDByteBuffer* input);
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

const RDXRefVect* rd_i_get_xrefs_from(RDContext* self, RDAddress fromaddr);
const RDXRefVect* rd_i_get_xrefs_from_type(RDContext* self, RDAddress fromaddr,
                                           usize type);
const RDXRefVect* rd_i_get_xrefs_to(RDContext* self, RDAddress toaddr);
const RDXRefVect* rd_i_get_xrefs_to_type(RDContext* self, RDAddress toaddr,
                                         usize type);

const RDXRefVect* rd_i_get_xrefs_from_ex(RDContext* self, RDAddress fromaddr,
                                         RDXRefVect* r);
const RDXRefVect* rd_i_get_xrefs_from_type_ex(RDContext* self,
                                              RDAddress fromaddr, usize type,
                                              RDXRefVect* r);
const RDXRefVect* rd_i_get_xrefs_to_ex(RDContext* self, RDAddress toaddr,
                                       RDXRefVect* r);
const RDXRefVect* rd_i_get_xrefs_to_type_ex(RDContext* self, RDAddress toaddr,
                                            usize type, RDXRefVect* r);

void rd_i_add_problem(RDContext* self, RDAddress from, RDAddress address,
                      const char* fmt, ...);
