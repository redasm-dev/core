#pragma once

#include "core/db/types.h"
#include "core/segment.h"
#include "types/def.h"
#include "types/type.h"
#include <redasm/redasm.h>
#include <sqlite3.h>
#include <stdbool.h>

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
    RD_QUERY_GET_XREFS_FROM_TYPE,
    RD_QUERY_GET_XREFS_FROM,
    RD_QUERY_GET_XREFS_TO_TYPE,
    RD_QUERY_GET_XREFS_TO,
    RD_QUERY_SET_COMMENT,
    RD_QUERY_GET_COMMENT,
    RD_QUERY_DEL_COMMENT,
    RD_QUERY_SET_TYPEDEF,
    RD_QUERY_SET_TYPEDEF_PARAMS,
    RD_QUERY_SET_TYPEDEF_ENUM,
    RD_QUERY_SET_TYPEDEF_FUNC,
    RD_QUERY_GET_TYPEDEF,
    RD_QUERY_GET_TYPEDEF_COMPOUND,
    RD_QUERY_GET_TYPEDEF_ENUM,
    RD_QUERY_SET_TYPE,
    RD_QUERY_GET_TYPE,
    RD_QUERY_DEL_TYPE,
    RD_QUERY_DEL_TYPE_RANGE,
    RD_QUERY_ADD_SEGMENT,
    RD_QUERY_ADD_MAPPING,
    RD_QUERY_SET_IMPORTED,
    RD_QUERY_GET_IMPORTED,
    RD_QUERY_SET_REGVAL,
    RD_QUERY_GET_REGVAL,
    RD_QUERY_GET_REGVAL_EXACT,
    RD_QUERY_ADD_PROBLEM,
    RD_QUERY_SET_USERDATA,
    RD_QUERY_GET_USERDATA,

    RD_QUERY_COUNT,
};

typedef struct RDDB {
    char* filepath;
    sqlite3* handle;
    sqlite3_stmt* queries[RD_QUERY_COUNT];
} RDDB;

typedef struct RDXRefVect {
    RDXRef* data;
    usize length;
    usize capacity;
} RDXRefVect;

bool rd_i_db_init(RDContext* ctx);
void rd_i_db_deinit(RDContext* ctx);
void rd_i_db_add_segment(RDContext* ctx, const RDSegmentFull* s);
void rd_i_db_add_mapping(RDContext* ctx, const RDInputMapping* m);

void rd_i_db_set_entry_point(RDContext* ctx, RDAddress address);
bool rd_i_db_get_entry_point(RDContext* ctx, RDAddress* address);

void rd_i_db_set_imported(RDContext* ctx, RDAddress address,
                          const RDImported* imp);
bool rd_i_db_get_imported(RDContext* ctx, RDAddress address, RDImported* imp);

void rd_i_db_add_xref(RDContext* ctx, RDAddress from, RDAddress to, usize t);
void rd_i_db_get_xrefs_from_type(RDContext* ctx, RDAddress from, usize t,
                                 RDXRefVect* refs);
void rd_i_db_get_xrefs_from(RDContext* ctx, RDAddress from, RDXRefVect* refs);
void rd_i_db_get_xrefs_to_type(RDContext* ctx, RDAddress to, usize t,
                               RDXRefVect* refs);
void rd_i_db_get_xrefs_to(RDContext* ctx, RDAddress to, RDXRefVect* refs);

bool rd_i_db_get_address(RDContext* ctx, const char* name, RDAddress* address);
bool rd_i_db_get_name(RDContext* ctx, RDAddress address, RDName* n);
void rd_i_db_set_name(RDContext* ctx, RDAddress address, const char* name,
                      RDConfidence c);
void rd_i_db_del_name(RDContext* ctx, RDAddress address);

void rd_i_db_set_type_def(RDContext* ctx, const RDTypeDef* tdef);

void rd_i_db_set_type(RDContext* ctx, RDAddress address, const char* name,
                      usize count, RDTypeFlags flags, RDConfidence c);
bool rd_i_db_get_type(RDContext* ctx, RDAddress address, RDTypeFull* t);
void rd_i_db_del_type(RDContext* ctx, RDAddress address);
void rd_i_db_del_type_range(RDContext* ctx, RDAddress startaddr,
                            RDAddress endaddr);

const char* rd_i_db_get_comment(RDContext* ctx, RDAddress address);
void rd_i_db_set_comment(RDContext* ctx, RDAddress address, const char* cmt);
void rd_i_db_del_comment(RDContext* ctx, RDAddress address);

void rd_i_db_add_problem(RDContext* ctx, const RDProblem* p);

bool rd_i_db_get_userdata(RDContext* ctx, const char* key, uptr* ud);
void rd_i_db_set_userdata(RDContext* ctx, const char* key, uptr ud);

void rd_i_db_set_regval(RDContext* ctx, RDAddress address, int reg, u64 val,
                        RDConfidence c);
bool rd_i_db_get_regval(RDContext* ctx, RDAddress address, int reg,
                        RDRegister* r);
bool rd_i_db_get_regval_exact(RDContext* ctx, RDAddress address, int reg,
                              RDRegister* r);
