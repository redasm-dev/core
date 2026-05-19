#pragma once

#include "core/segment.h"
#include "db/types.h"
#include "types/def.h"
#include "types/type.h"
#include <redasm/context.h>

enum {
    RD_QUERY_SET_NAME = 0,
    RD_QUERY_GET_NAME,
    RD_QUERY_DEL_NAME,
    RD_QUERY_GET_ADDRESS,

    RD_QUERY_SET_INFO_INT,
    RD_QUERY_GET_INFO_INT,
    RD_QUERY_SET_INFO_STR,
    RD_QUERY_GET_INFO_STR,

    RD_QUERY_ADD_XREF,
    RD_QUERY_DEL_XREF,
    RD_QUERY_GET_XREF,

    RD_QUERY_GET_XREFS_FROM,
    RD_QUERY_GET_XREFS_TO,
    RD_QUERY_DEL_XREFS_FROM,
    RD_QUERY_DEL_XREFS_TO,

    RD_QUERY_HAS_XREFS_FROM,
    RD_QUERY_HAS_XREFS_TO,

    RD_QUERY_SET_COMMENT,
    RD_QUERY_GET_COMMENT,
    RD_QUERY_DEL_COMMENT,

    RD_QUERY_SET_TYPEDEF,
    RD_QUERY_SET_TYPEDEF_PARAMS,
    RD_QUERY_GET_TYPEDEF,
    RD_QUERY_GET_TYPEDEF_PARAMS,

    RD_QUERY_SET_TYPE,
    RD_QUERY_GET_TYPE,
    RD_QUERY_DEL_TYPE,
    RD_QUERY_DEL_TYPE_RANGE, // ?!?

    RD_QUERY_ADD_SEGMENT,
    RD_QUERY_ADD_MAPPING,

    RD_QUERY_SET_IMPORTED,
    RD_QUERY_GET_IMPORTED,

    RD_QUERY_SET_REGVAL,

    RD_QUERY_SET_OVR_OPERAND,
    RD_QUERY_DEL_OVR_OPERAND,
    RD_QUERY_HAS_OVR_OPERAND,

    RD_QUERY_GET_ALL_OVR_OPERAND,

    RD_QUERY_ADD_PROBLEM,

    RD_QUERY_SET_USERDATA,
    RD_QUERY_GET_USERDATA,

    RD_QUERY_GET_MAX_CONFIDENCE,

    RD_QUERY_COUNT,
};

void _rd_i_db_query_set_info_int(RDContext* ctx, const char* key, u64 val);
bool _rd_i_db_query_get_info_int(RDContext* ctx, const char* key, u64* val);
void _rd_i_db_query_set_info_str(RDContext* ctx, const char* key,
                                 const char* val);
bool _rd_db_i_query_get_info_str(RDContext* ctx, const char* key,
                                 const char** val);

void _rd_i_db_query_add_segment(RDContext* ctx, const RDSegmentFull* s);
void _rd_i_db_query_add_mapping(RDContext* ctx, const RDInputMapping* m);

void _rd_i_db_query_set_imported(RDContext* ctx, RDAddress address,
                                 const RDImported* imp);
bool _rd_i_db_query_get_imported(RDContext* ctx, RDAddress address,
                                 RDImported* imp);

void _rd_i_db_query_add_xref(RDContext* ctx, RDAddress from, RDAddress to,
                             RDXRefType type, RDConfidence c);
bool _rd_i_db_query_del_xref(RDContext* ctx, RDAddress from, RDAddress to,
                             RDConfidence c);
void _rd_i_db_query_del_xref_from(RDContext* ctx, RDAddress from, RDAddress to,
                                  RDXRefType type, RDConfidence c);
bool _rd_i_db_query_get_xref(RDContext* ctx, RDAddress from, RDAddress to,
                             RDXRefFull* xref);
RDXRefVect* _rd_i_db_query_get_xrefs_from(RDContext* ctx, RDAddress from,
                                          RDXRefType type, RDXRefVect* refs);
RDXRefVect* _rd_i_db_query_get_xrefs_to(RDContext* ctx, RDAddress to,
                                        RDXRefType type, RDXRefVect* refs);
bool _rd_i_db_query_del_xrefs_from(RDContext* ctx, RDAddress from,
                                   RDConfidence c);
bool _rd_i_db_query_del_xrefs_to(RDContext* ctx, RDAddress to, RDConfidence c);
bool _rd_i_db_query_has_xrefs_from(RDContext* ctx, RDAddress address);
bool _rd_i_db_query_has_xrefs_to(RDContext* ctx, RDAddress address);

bool _rd_i_db_query_get_address(RDContext* ctx, const char* name,
                                RDAddress* address);
bool _rd_i_db_query_get_name(RDContext* ctx, RDAddress address, RDName* n);
void _rd_i_db_query_set_name(RDContext* ctx, RDAddress address,
                             const char* name, RDConfidence c);
bool _rd_i_db_query_del_name(RDContext* ctx, RDAddress address);

void _rd_i_db_query_set_type(RDContext* ctx, RDAddress address,
                             const char* name, usize count, RDTypeModifier mod,
                             RDConfidence c);
bool _rd_i_db_query_get_type(RDContext* ctx, RDAddress address, RDTypeFull* t);
bool _rd_i_db_query_del_type(RDContext* ctx, RDAddress address);
void _rd_i_db_query_del_type_range(RDContext* ctx, RDAddress startaddr,
                                   RDAddress endaddr);
void _rd_i_db_query_set_type_def(RDContext* ctx, const RDTypeDef* tdef);

const char* _rd_i_db_query_get_comment(RDContext* ctx, RDAddress address);
void _rd_i_db_query_set_comment(RDContext* ctx, RDAddress address,
                                const char* cmt);
void _rd_i_db_query_del_comment(RDContext* ctx, RDAddress address);

void _rd_i_db_query_add_problem(RDContext* ctx, const RDProblem* p);

bool _rd_i_db_query_get_userdata(RDContext* ctx, const char* key, uptr* ud);
void _rd_i_db_query_set_userdata(RDContext* ctx, const char* key, uptr ud);

void _rd_i_db_query_set_regval(RDContext* ctx, RDAddress address,
                               const char* regname, RDRegValue val,
                               RDConfidence c);

void _rd_i_db_query_set_ovr_operand(RDContext* ctx, RDAddress address, int idx);
void _rd_i_db_query_del_ovr_operand(RDContext* ctx, RDAddress address, int idx);
bool _rd_i_db_query_has_ovr_operand(RDContext* ctx, RDAddress address);
RDOvrOperandVect* _rd_i_db_query_get_all_ovr_operand(RDContext* ctx,
                                                     RDAddress address);

RDConfidence _rd_i_db_query_get_max_confidence(RDContext* ctx, RDAddress start,
                                               RDAddress end);
