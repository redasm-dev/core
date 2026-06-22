#pragma once

#include "core/segment.h"
#include "db/queries.h"
#include "db/types.h"
#include <redasm/redasm.h>
#include <sqlite3.h>

typedef struct RDDB {
    RDSegmentFullVect segments;
    RDMappingVect mappings;
    RDSegmentRegsVect segment_regs;
    RDSegmentRegNameVect segment_reg_names;

    RDSegmentFull* last_segment;

    char* filepath;
    sqlite3* handle;
    sqlite3_stmt* queries[RD_QUERY_COUNT];
} RDDB;

bool rd_i_db_is_valid(const char* dbpath);

RDDB* rd_i_db_create(const char* dbpath);
void rd_i_db_destroy(RDDB* self);
void rd_i_db_begin(RDContext* ctx);
void rd_i_db_commit(RDContext* ctx);
void rd_i_db_rollback(RDContext* ctx);
void rd_i_db_save(RDContext* ctx);
void rd_i_db_load(RDContext* ctx);
bool rd_i_db_export(RDContext* ctx, const char* filepath);

bool rd_i_db_add_segment(RDContext* ctx, RDSegmentFull* seg);
const RDSegmentFull* rd_i_db_find_segment(const RDContext* ctx,
                                          RDAddress address);
const RDSegmentFullVect* rd_i_db_get_segments(const RDContext* ctx);

bool rd_i_db_add_mapping(RDContext* ctx, RDInputMapping m);
const RDMappingVect* rd_i_db_get_mappings(const RDContext* ctx);

void rd_i_db_set_external(RDContext* ctx, const RDExternal* exp);
RDExternalVect* rd_i_db_get_externals(RDContext* ctx, RDExternalKind kind,
                                      RDExternalVect* v);

void rd_i_db_add_xref(RDContext* ctx, RDAddress from, RDAddress to,
                      RDXRefType t, RDConfidence c);
bool rd_i_db_del_xref(RDContext* ctx, RDAddress from, RDAddress to,
                      RDConfidence c);
bool rd_i_db_get_xref(RDContext* ctx, RDAddress from, RDAddress to,
                      RDXRefFull* xref);
RDXRefVect* rd_i_db_get_xrefs_from(RDContext* ctx, RDAddress from,
                                   RDXRefType type, RDXRefVect* refs);
RDXRefVect* rd_i_db_get_xrefs_to(RDContext* ctx, RDAddress to, RDXRefType type,
                                 RDXRefVect* refs);
bool rd_i_db_del_xrefs_from(RDContext* ctx, RDAddress from, RDConfidence c);
bool rd_i_db_del_xrefs_to(RDContext* ctx, RDAddress to, RDConfidence c);
bool rd_i_db_has_xrefs_from(RDContext* ctx, RDAddress address);
bool rd_i_db_has_xrefs_to(RDContext* ctx, RDAddress address);

bool rd_i_db_get_address(RDContext* ctx, const char* name, RDAddress* address);
bool rd_i_db_get_name(RDContext* ctx, RDAddress address, RDName* n);
void rd_i_db_set_name(RDContext* ctx, RDAddress address, const char* name,
                      RDConfidence c);
bool rd_i_db_del_name(RDContext* ctx, RDAddress address);

void rd_i_db_set_type_def(RDContext* ctx, const RDTypeDef* tdef);

void rd_i_db_set_type(RDContext* ctx, RDAddress address, const char* name,
                      usize count, RDTypeModifier mod, RDConfidence c);
bool rd_i_db_get_type(RDContext* ctx, RDAddress address, RDTypeFull* t);
bool rd_i_db_del_type(RDContext* ctx, RDAddress address);

const char* rd_i_db_get_comment(RDContext* ctx, RDAddress address);
void rd_i_db_set_comment(RDContext* ctx, RDAddress address, const char* cmt);
void rd_i_db_del_comment(RDContext* ctx, RDAddress address);

void rd_i_db_add_problem(RDContext* ctx, const RDProblem* p);

bool rd_i_db_set_sregval(RDContext* ctx, RDAddress address, const char* regname,
                         RDRegValue val, RDConfidence c);
bool rd_i_db_del_sregval(RDContext* ctx, RDAddress address, const char* regname,
                         RDConfidence c);
bool rd_i_db_get_sregval(RDContext* ctx, RDAddress address, const char* regname,
                         RDRegValue* value);
const RDSegmentRegNameVect* rd_i_db_get_all_sreg_names(const RDContext* ctx);
const RDSegmentRegVect* rd_i_db_get_all_sregval(RDContext* ctx,
                                                const char* regname);

void rd_i_db_set_ovr_operand(RDContext* ctx, RDAddress address, int idx);
void rd_i_db_del_ovr_operand(RDContext* ctx, RDAddress address, int idx);
bool rd_i_db_has_ovr_operand(RDContext* ctx, RDAddress address);

RDOvrOperandVect* rd_i_db_get_all_ovr_operand(RDContext* ctx,
                                              RDAddress address);

RDConfidence rd_i_db_get_undefine_confidence(RDContext* ctx, RDAddress start,
                                             RDAddress end);
