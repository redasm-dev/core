#include "queries.h"
#include "core/context.h"
#include "db/types.h"
#include "support/containers.h"
#include "support/error.h"
#include "support/stringpool.h"
#include <sqlite3.h>
#include <string.h>

#define RD_DB_BIND(ctx, stmt, id, bind_call)                                   \
    do {                                                                       \
        int _idx = sqlite3_bind_parameter_index((stmt), (id));                 \
        panic_if(!_idx, "SQL: parameter '%s' not found", (id));                \
        int _res = (bind_call);                                                \
        panic_if(_res != SQLITE_OK, "SQL: %s",                                 \
                 sqlite3_errmsg((ctx)->db->handle));                           \
    } while(0)

static void _rd_db_bind_param_null(const RDContext* ctx, sqlite3_stmt* stmt,
                                   const char* id) {
    RD_DB_BIND(ctx, stmt, id, sqlite3_bind_null(stmt, _idx));
}

static void _rd_db_bind_param_int(const RDContext* ctx, sqlite3_stmt* stmt,
                                  const char* id, sqlite3_int64 v) {
    RD_DB_BIND(ctx, stmt, id, sqlite3_bind_int64(stmt, _idx, v));
}

static void _rd_db_bind_param_str(const RDContext* ctx, sqlite3_stmt* stmt,
                                  const char* id, const char* s) {
    RD_DB_BIND(ctx, stmt, id,
               sqlite3_bind_text(stmt, _idx, s, -1, SQLITE_STATIC));
}

static int _rd_db_step(const RDContext* ctx, sqlite3_stmt* stmt) {
    int res = sqlite3_step(stmt);
    if((res == SQLITE_ROW) || (res == SQLITE_DONE)) return res;
    panic("SQL: %s", sqlite3_errmsg(ctx->db->handle));
}

static sqlite3_stmt* _rd_db_prepare_query(RDContext* ctx, int id,
                                          const char* q) {
    if(!ctx->db->queries[id]) {
        sqlite3_stmt* stmt = NULL;
        int rc =
            sqlite3_prepare_v2(ctx->db->handle, q, (int)strlen(q), &stmt, NULL);

        if(rc != SQLITE_OK)
            panic("SQL: %s\nQUERY: %s", sqlite3_errmsg(ctx->db->handle), q);

        assert(stmt && "statement is NULL");
        ctx->db->queries[id] = stmt;
    }
    else
        sqlite3_reset(ctx->db->queries[id]);

    return ctx->db->queries[id];
}

static sqlite3_stmt*
_rd_db_prepare_set_type_def_params_query(RDContext* ctx, const char* owner,
                                         const RDType* type, const char* name,
                                         usize member_idx) {
    sqlite3_stmt* stmt =
        _rd_db_prepare_query(ctx, RD_QUERY_SET_TYPE_DEF_PARAMS, "\
        INSERT INTO TypeParams \
            VALUES (:owner, :type, :name, :count, :modifier, :member_idx) \
        ON CONFLICT DO \
            UPDATE SET type = EXCLUDED.type, \
                       name = EXCLUDED.name, \
                       count = EXCLUDED.count, \
                       modifier = EXCLUDED.modifier, \
                       member_idx = EXCLUDED.member_idx \
    ");

    _rd_db_bind_param_str(ctx, stmt, ":owner", owner);
    _rd_db_bind_param_int(ctx, stmt, ":member_idx", (sqlite3_int64)member_idx);

    _rd_db_bind_param_str(ctx, stmt, ":type", type->name);
    _rd_db_bind_param_int(ctx, stmt, ":count", (sqlite3_int64)type->count);
    _rd_db_bind_param_int(ctx, stmt, ":modifier", (sqlite3_int64)type->mod);
    _rd_db_bind_param_str(ctx, stmt, ":name", name);

    return stmt;
}

static sqlite3_stmt* _rd_db_prepare_get_all_type_param(RDContext* ctx,
                                                       const char* owner) {
    sqlite3_stmt* stmt =
        _rd_db_prepare_query(ctx, RD_QUERY_GET_ALL_TYPE_PARAM, "\
        SELECT type, name, count, modifier, member_idx \
        FROM TypeParams \
        WHERE owner = :owner \
        ORDER BY member_idx \
    ");

    _rd_db_bind_param_str(ctx, stmt, ":owner", owner);
    return stmt;
}

static sqlite3_stmt* _rd_db_prepare_set_type_enum_query(RDContext* ctx,
                                                        const char* owner,
                                                        const char* name,
                                                        i64 value) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_SET_TYPE_ENUM, "\
        INSERT INTO TypeEnum \
            VALUES (:owner, :name, :value) \
        ON CONFLICT DO \
            UPDATE SET name = EXCLUDED.name, \
                       value = EXCLUDED.value \
    ");

    _rd_db_bind_param_str(ctx, stmt, ":owner", owner);
    _rd_db_bind_param_str(ctx, stmt, ":name", name);
    _rd_db_bind_param_int(ctx, stmt, ":value", (sqlite3_int64)value);
    return stmt;
}

static sqlite3_stmt* _rd_db_prepare_get_type_enum_params(RDContext* ctx,
                                                         const char* owner,
                                                         const char* name) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_GET_TYPE_ENUM, "\
        SELECT value \
        FROM TypeEnum \
        WHERE owner = :owner \
        AND name = :name \
    ");

    _rd_db_bind_param_str(ctx, stmt, ":owner", owner);
    _rd_db_bind_param_str(ctx, stmt, "name", name);
    return stmt;
}

void _rd_i_db_query_add_segment(RDContext* ctx, const RDSegmentFull* s) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_ADD_SEGMENT, "\
        INSERT INTO Segments \
        VALUES (:name, :startaddr, :endaddr, :unit, :perm) \
    ");

    _rd_db_bind_param_str(ctx, stmt, ":name", s->base.name);
    _rd_db_bind_param_int(ctx, stmt, ":startaddr",
                          (sqlite3_int64)s->base.start_address);
    _rd_db_bind_param_int(ctx, stmt, ":endaddr",
                          (sqlite3_int64)s->base.end_address);
    _rd_db_bind_param_int(ctx, stmt, ":unit", s->base.unit);
    _rd_db_bind_param_int(ctx, stmt, ":perm", s->base.perm);
    _rd_db_step(ctx, stmt);
}

RDSegmentFullVect* _rd_i_db_query_get_all_segments(RDContext* ctx,
                                                   RDSegmentFullVect* v) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_GET_ALL_SEGMENTS, "\
        SELECT name, start_address, end_address, unit, perm \
        FROM Segments \
        ORDER BY start_address \
    ");

    while(_rd_db_step(ctx, stmt) == SQLITE_ROW) {
        const char* name = (const char*)sqlite3_column_text(stmt, 0);
        RDAddress start = (RDAddress)sqlite3_column_int64(stmt, 1);
        RDAddress end = (RDAddress)sqlite3_column_int64(stmt, 2);
        u32 unit = (u32)sqlite3_column_int(stmt, 3);
        u32 perm = (u32)sqlite3_column_int(stmt, 4);

        RDSegmentFull* s =
            rd_i_segment_create(ctx, name, start, end, perm, unit);

        panic_if(!s, "segment '%s' loading failed", name);
        vect_push(v, s);
    }

    return v;
}

void _rd_i_db_query_add_mapping(RDContext* ctx, const RDInputMapping* m) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_ADD_MAPPING, "\
        INSERT INTO InputMappings \
        VALUES (:offset, :startaddr, :endaddr) \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":offset", (sqlite3_int64)m->offset);
    _rd_db_bind_param_int(ctx, stmt, ":startaddr",
                          (sqlite3_int64)m->start_address);
    _rd_db_bind_param_int(ctx, stmt, ":endaddr", (sqlite3_int64)m->end_address);
    _rd_db_step(ctx, stmt);
}

RDMappingVect* _rd_i_db_query_get_all_mappings(RDContext* ctx,
                                               RDMappingVect* v) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_GET_ALL_MAPPINGS, "\
        SELECT offset, start_address, end_address \
        FROM InputMappings \
        ORDER BY start_address \
    ");

    while(_rd_db_step(ctx, stmt) == SQLITE_ROW) {
        RDOffset offset = (RDOffset)sqlite3_column_int64(stmt, 0);
        RDAddress start = (RDAddress)sqlite3_column_int64(stmt, 1);
        RDAddress end = (RDAddress)sqlite3_column_int64(stmt, 2);

        vect_push(v, (RDInputMapping){
                         .offset = offset,
                         .start_address = start,
                         .end_address = end,
                     });
    }

    return v;
}

void _rd_i_db_query_set_external(RDContext* ctx, const RDExternal* ext) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_SET_EXTERNAL, "\
        INSERT INTO Externals \
            VALUES(:address, :ordinal, :module, :kind) \
        ON CONFLICT DO \
            UPDATE SET ordinal = EXCLUDED.ordinal, \
                       module  = EXCLUDED.module, \
                       kind    = EXCLUDED.kind \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":kind", (sqlite3_int64)ext->kind);
    _rd_db_bind_param_int(ctx, stmt, ":address", (sqlite3_int64)ext->address);

    if(ext->ordinal.has_value)
        _rd_db_bind_param_int(ctx, stmt, ":ordinal", ext->ordinal.value);
    else
        _rd_db_bind_param_null(ctx, stmt, ":ordinal");

    if(ext->module)
        _rd_db_bind_param_str(ctx, stmt, ":module", ext->module);
    else
        _rd_db_bind_param_null(ctx, stmt, ":module");

    _rd_db_step(ctx, stmt);
}

RDExternalVect* _rd_i_db_query_get_all_externals(RDContext* ctx,
                                                 RDExternalKind kind,
                                                 RDExternalVect* v) {
    sqlite3_stmt* stmt =
        _rd_db_prepare_query(ctx, RD_QUERY_GET_ALL_EXTERNALS, "\
        SELECT address, module, ordinal, kind \
        FROM Externals \
        WHERE (:kind = 0 OR kind = :kind) \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":kind", (sqlite3_int64)kind);

    vect_clear(v);

    while(_rd_db_step(ctx, stmt) == SQLITE_ROW) {
        RDExternal ext = {
            .kind = (RDExternalKind)sqlite3_column_int64(stmt, 3),
            .address = (RDAddress)sqlite3_column_int64(stmt, 0),
            .module = rd_i_strpool_intern(&ctx->strings,
                                          (char*)sqlite3_column_text(stmt, 1)),
        };

        if(sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
            ext.ordinal.value = (u32)sqlite3_column_int64(stmt, 2);
            ext.ordinal.has_value = true;
        }
        else
            ext.ordinal.has_value = false;

        if(ext.kind == RD_EXT_EXPORTED && !ext.module)
            ext.module = ctx->file_name;

        vect_push(v, ext);
    }

    return v;
}

void _rd_i_db_query_add_xref(RDContext* ctx, RDAddress from, RDAddress to,
                             RDXRefType type, RDConfidence c) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_ADD_XREF, "\
        INSERT INTO XRefs \
                VALUES (:fromaddr, :toaddr, :type, :confidence) \
            ON CONFLICT DO \
                UPDATE SET type = EXCLUDED.type, \
                           confidence = EXCLUDED.confidence \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":fromaddr", (sqlite3_int64)from);
    _rd_db_bind_param_int(ctx, stmt, ":toaddr", (sqlite3_int64)to);
    _rd_db_bind_param_int(ctx, stmt, ":type", type);
    _rd_db_bind_param_int(ctx, stmt, ":confidence", c);
    _rd_db_step(ctx, stmt);
}

bool _rd_i_db_query_del_xref(RDContext* ctx, RDAddress from, RDAddress to,
                             RDConfidence c) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_DEL_XREF, "\
            DELETE FROM XRefs \
            WHERE from_address = :fromaddr \
            AND to_address = :toaddr \
            AND confidence <= :confidence \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":fromaddr", (sqlite3_int64)from);
    _rd_db_bind_param_int(ctx, stmt, ":toaddr", (sqlite3_int64)to);
    _rd_db_bind_param_int(ctx, stmt, ":confidence", c);
    _rd_db_step(ctx, stmt);

    return sqlite3_changes(ctx->db->handle) > 0;
}

bool _rd_i_db_query_get_xref(RDContext* ctx, RDAddress from, RDAddress to,
                             RDXRefFull* xref) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_GET_XREF, "\
            SELECT from_address, to_address, type, confidence  \
            FROM XRefs \
            WHERE from_address = :fromaddr \
            AND to_address = :toaddr \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":fromaddr", (sqlite3_int64)from);
    _rd_db_bind_param_int(ctx, stmt, ":toaddr", (sqlite3_int64)to);

    if(_rd_db_step(ctx, stmt) == SQLITE_ROW) {
        xref->base.address = (RDAddress)sqlite3_column_int64(stmt, 1);
        xref->base.type = (RDXRefType)sqlite3_column_int64(stmt, 2);
        xref->from_address = (RDConfidence)sqlite3_column_int(stmt, 0);
        xref->confidence = (RDConfidence)sqlite3_column_int(stmt, 3);
        return true;
    }

    return false;
}

RDXRefVect* _rd_i_db_query_get_xrefs_from(RDContext* ctx, RDAddress from,
                                          RDXRefType type, RDXRefVect* refs) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_GET_XREFS_FROM, "\
        SELECT to_address, type  \
        FROM XRefs \
        WHERE from_address = :fromaddr \
        AND (:type = 0 OR type = :type) \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":fromaddr", (sqlite3_int64)from);
    _rd_db_bind_param_int(ctx, stmt, ":type", type);

    vect_clear(refs);

    while(_rd_db_step(ctx, stmt) == SQLITE_ROW) {
        vect_push(refs, (RDXRef){
                            .address = (RDAddress)sqlite3_column_int64(stmt, 0),
                            .type = (RDXRefType)sqlite3_column_int64(stmt, 1),
                        });
    }

    return refs;
}

RDXRefVect* _rd_i_db_query_get_xrefs_to(RDContext* ctx, RDAddress to,
                                        RDXRefType type, RDXRefVect* refs) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_GET_XREFS_TO, "\
        SELECT from_address, type  \
        FROM XRefs \
        WHERE to_address = :toaddr \
        AND (:type = 0 OR type = :type) \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":toaddr", (sqlite3_int64)to);
    _rd_db_bind_param_int(ctx, stmt, ":type", (sqlite3_int64)type);

    vect_clear(refs);

    while(_rd_db_step(ctx, stmt) == SQLITE_ROW) {
        vect_push(refs, (RDXRef){
                            .address = (RDAddress)sqlite3_column_int64(stmt, 0),
                            .type = (RDXRefType)sqlite3_column_int64(stmt, 1),
                        });
    }

    return refs;
}

bool _rd_i_db_query_del_xrefs_from(RDContext* ctx, RDAddress from,
                                   RDConfidence c) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_DEL_XREFS_FROM, "\
            DELETE FROM XRefs \
            WHERE from_address = :fromaddr \
            AND confidence <= :confidence \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":fromaddr", (sqlite3_int64)from);
    _rd_db_bind_param_int(ctx, stmt, ":confidence", c);
    _rd_db_step(ctx, stmt);

    return sqlite3_changes(ctx->db->handle) > 0;
}

bool _rd_i_db_query_del_xrefs_to(RDContext* ctx, RDAddress to, RDConfidence c) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_DEL_XREFS_TO, "\
            DELETE FROM XRefs \
            WHERE to_address = :toaddr \
            AND confidence <= :confidence \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":toaddr", (sqlite3_int64)to);
    _rd_db_bind_param_int(ctx, stmt, ":confidence", c);
    _rd_db_step(ctx, stmt);

    return sqlite3_changes(ctx->db->handle) > 0;
}

bool _rd_i_db_query_has_xrefs_from(RDContext* ctx, RDAddress address) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(
        ctx, RD_QUERY_HAS_XREFS_FROM, "SELECT EXISTS(SELECT 1 FROM XRefs \
                                WHERE from_address = :address)");

    if(!stmt) return false;
    _rd_db_bind_param_int(ctx, stmt, ":address", (sqlite3_int64)address);

    bool r = false;
    if(sqlite3_step(stmt) == SQLITE_ROW) r = sqlite3_column_int(stmt, 0);
    return r;
}

bool _rd_i_db_query_has_xrefs_to(RDContext* ctx, RDAddress address) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(
        ctx, RD_QUERY_HAS_XREFS_TO,
        "SELECT EXISTS(SELECT 1 FROM XRefs WHERE to_address = :address)");

    if(!stmt) return false;
    _rd_db_bind_param_int(ctx, stmt, ":address", (sqlite3_int64)address);

    bool r = false;
    if(sqlite3_step(stmt) == SQLITE_ROW) r = sqlite3_column_int(stmt, 0);
    return r;
}

bool _rd_i_db_query_get_address(RDContext* ctx, const char* name,
                                RDAddress* address) {
    assert(address && "address is NULL");
    assert(name && "name is NULL");

    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_GET_ADDRESS, "\
        SELECT address \
        FROM Names \
        WHERE name = :name \
    ");

    _rd_db_bind_param_str(ctx, stmt, ":name", name);

    if(_rd_db_step(ctx, stmt) == SQLITE_ROW) {
        *address = (RDAddress)sqlite3_column_int64(stmt, 0);
        return true;
    }

    return false;
}

bool _rd_i_db_query_get_name(RDContext* ctx, RDAddress address, RDName* n) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_GET_NAME, "\
        SELECT name, confidence \
        FROM Names \
        WHERE address = :address \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":address", (sqlite3_int64)address);

    if(_rd_db_step(ctx, stmt) == SQLITE_ROW) {
        n->value = (const char*)sqlite3_column_text(stmt, 0);
        n->confidence = (RDConfidence)sqlite3_column_int64(stmt, 1);
        return true;
    }

    return false;
}

void _rd_i_db_query_set_name(RDContext* ctx, RDAddress address,
                             const char* name, RDConfidence c) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_SET_NAME, "\
        INSERT INTO Names \
            VALUES (:address, :name, :confidence) \
        ON CONFLICT DO  \
            UPDATE SET name = EXCLUDED.name, \
                       confidence = EXCLUDED.confidence\
    ");

    _rd_db_bind_param_int(ctx, stmt, ":address", (sqlite3_int64)address);
    _rd_db_bind_param_str(ctx, stmt, ":name", name);
    _rd_db_bind_param_int(ctx, stmt, ":confidence", c);
    _rd_db_step(ctx, stmt);
}

bool _rd_i_db_query_del_name(RDContext* ctx, RDAddress address) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_DEL_NAME, "\
        DELETE FROM Names \
        WHERE address = :address \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":address", (sqlite3_int64)address);
    _rd_db_step(ctx, stmt);

    return sqlite3_changes(ctx->db->handle) > 0;
}

void _rd_i_db_query_set_type(RDContext* ctx, RDAddress address,
                             const char* name, usize count, RDTypeModifier mod,
                             RDConfidence c) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_SET_TYPE, "\
        INSERT INTO Types \
            VALUES (:address, :name, :count, :mod, :confidence) \
        ON CONFLICT DO  \
            UPDATE SET name = EXCLUDED.name,  \
                       count = EXCLUDED.count, \
                       modifier = EXCLUDED.modifier, \
                       confidence = EXCLUDED.confidence \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":address", (sqlite3_int64)address);
    _rd_db_bind_param_str(ctx, stmt, ":name", name);
    _rd_db_bind_param_int(ctx, stmt, ":count", (sqlite3_int64)count);
    _rd_db_bind_param_int(ctx, stmt, ":mod", mod);
    _rd_db_bind_param_int(ctx, stmt, ":confidence", c);
    _rd_db_step(ctx, stmt);
}

bool _rd_i_db_query_get_type(RDContext* ctx, RDAddress address, RDTypeFull* t) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_GET_TYPE, "\
        SELECT name, count, modifier, confidence  \
        FROM Types \
        WHERE address = :address \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":address", (sqlite3_int64)address);

    if(_rd_db_step(ctx, stmt) == SQLITE_ROW) {
        t->base.name = rd_i_strpool_intern(&ctx->strings,
                                           (char*)sqlite3_column_text(stmt, 0));
        t->base.count = (usize)sqlite3_column_int64(stmt, 1);
        t->base.mod = (RDTypeModifier)sqlite3_column_int64(stmt, 2);
        t->confidence = (RDConfidence)sqlite3_column_int64(stmt, 3);
        return true;
    }

    return false;
}

bool _rd_i_db_query_del_type(RDContext* ctx, RDAddress address) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_DEL_TYPE, "\
        DELETE FROM Types \
            WHERE address = :address \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":address", (sqlite3_int64)address);
    _rd_db_step(ctx, stmt);

    return sqlite3_changes(ctx->db->handle) > 0;
}

RDTypeFullVect* _rd_i_db_query_get_all_types(RDContext* ctx, RDAddressVect* av,
                                             RDTypeFullVect* v) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_GET_ALL_TYPES, "\
        SELECT address, name, count, modifier, confidence \
        FROM Types \
        ORDER BY address \
    ");

    vect_clear(av);
    vect_clear(v);

    while(_rd_db_step(ctx, stmt) == SQLITE_ROW) {
        RDAddress address = (RDAddress)sqlite3_column_int64(stmt, 0);
        usize count = (usize)sqlite3_column_int64(stmt, 1);
        const char* name = (const char*)sqlite3_column_text(stmt, 2);
        RDTypeModifier mod = (RDTypeModifier)sqlite3_column_int(stmt, 3);
        RDConfidence c = (RDConfidence)sqlite3_column_int(stmt, 4);

        vect_push(av, address);

        vect_push(v,
                  (RDTypeFull){
                      .base =
                          {
                              .mod = mod,
                              .name = rd_i_strpool_intern(&ctx->strings, name),
                              .count = count,
                          },
                      .confidence = c,
                  });
    }

    assert(vect_length(av) == vect_length(v));
    return v;
}

void _rd_i_db_query_set_type_def(RDContext* ctx, const RDTypeDef* tdef) {
    if(tdef->kind == RD_TKIND_PRIM) return;

    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_SET_TYPE_DEF, "\
        INSERT INTO TypeDefs \
            VALUES (:name, :kind, :size, :is_noret, :enum_type) \
        ON CONFLICT DO \
            UPDATE SET kind = EXCLUDED.kind, \
                       size = EXCLUDED.size, \
                       is_noret = EXCLUDED.is_noret, \
                       enum_type = EXCLUDED.enum_type \
    ");

    _rd_db_bind_param_str(ctx, stmt, ":name", tdef->name);
    _rd_db_bind_param_int(ctx, stmt, ":kind", (sqlite3_int64)tdef->kind);
    _rd_db_bind_param_int(ctx, stmt, ":size", (sqlite3_int64)tdef->size);

    if(tdef->kind == RD_TKIND_FUNC)
        _rd_db_bind_param_int(ctx, stmt, ":is_noret", tdef->func_.is_noret);
    else
        _rd_db_bind_param_int(ctx, stmt, ":is_noret", 0);

    if(tdef->kind == RD_TKIND_ENUM)
        _rd_db_bind_param_str(ctx, stmt, ":enum_type", tdef->enum_.base_type);
    else
        _rd_db_bind_param_null(ctx, stmt, ":enum_type");

    _rd_db_step(ctx, stmt);

    if(rd_i_typedef_is_compound(tdef)) {
        const RDParam* m;
        usize i = 0;
        vect_each(m, &tdef->compound_) {
            stmt = _rd_db_prepare_set_type_def_params_query(
                ctx, tdef->name, &m->type, m->name, i++);
            _rd_db_step(ctx, stmt);
        }
    }
    else if(tdef->kind == RD_TKIND_ENUM) {
        const RDEnumCase* c;
        vect_each(c, &tdef->enum_) {
            stmt = _rd_db_prepare_set_type_enum_query(ctx, tdef->name, c->name,
                                                      c->value);
            _rd_db_step(ctx, stmt);
        }
    }
    else if(tdef->kind == RD_TKIND_FUNC) {
        if(!rd_type_is_void(&tdef->func_.ret)) {
            // return type at member_idx=0 and has no name
            stmt = _rd_db_prepare_set_type_def_params_query(
                ctx, tdef->name, &tdef->func_.ret, "", 0);
            _rd_db_step(ctx, stmt);
        }

        // args type at member_idx=1..n
        const RDParam* p;
        usize i = 1;
        vect_each(p, &tdef->func_.args) {
            stmt = _rd_db_prepare_set_type_def_params_query(
                ctx, tdef->name, &p->type, p->name, i++);
            _rd_db_step(ctx, stmt);
        }
    }
    else
        unreachable();
}

RDTypeDefVect* _rd_i_db_query_get_all_type_def(RDContext* ctx,
                                               RDTypeDefVect* v) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_GET_ALL_TYPE_DEF, "\
        SELECT name, kind, size, is_noret, enum_type \
        FROM TypeDefs \
    ");

    while(_rd_db_step(ctx, stmt) == SQLITE_ROW) {
        const char* name = (const char*)sqlite3_column_text(stmt, 0);
        RDTypeKind kind = (RDTypeKind)sqlite3_column_int(stmt, 1);
        usize size = (usize)sqlite3_column_int64(stmt, 2);
        bool is_noret = (bool)sqlite3_column_int(stmt, 3);
        const char* enum_type = (const char*)sqlite3_column_text(stmt, 4);

        sqlite3_stmt* stmt_param = _rd_db_prepare_get_all_type_param(ctx, name);
        RDTypeDef* tdef = NULL;

        if(kind == RD_TKIND_STRUCT)
            tdef = rd_typedef_create_struct(name, ctx);
        else if(kind == RD_TKIND_UNION)
            tdef = rd_typedef_create_union(name, ctx);
        else if(kind == RD_TKIND_ENUM)
            tdef = rd_typedef_create_enum(name, enum_type, ctx);
        else if(kind == RD_TKIND_FUNC) {
            tdef = rd_typedef_create_func(name, ctx);
            rd_typedef_set_noret(tdef, is_noret);
        }
        else
            unreachable();

        assert(tdef);
        tdef->size = size;

        while(_rd_db_step(ctx, stmt_param) == SQLITE_ROW) {
            // clang-format off
            const char* p_type = (const char*)sqlite3_column_text(stmt_param, 0);
            const char* p_name = (const char*)sqlite3_column_text(stmt_param, 1);
            usize p_count = (usize)sqlite3_column_int64(stmt_param, 2);
            RDTypeModifier p_mod = (RDTypeModifier)sqlite3_column_int64(stmt_param, 3);
            usize p_midx = (usize)sqlite3_column_int64(stmt_param, 3);
            // clang-format on

            if(rd_i_typedef_is_compound(tdef)) {
                rd_typedef_add_member(tdef, p_type, p_name, p_count, p_mod,
                                      ctx);
            }
            else if(kind == RD_TKIND_ENUM) {
                sqlite3_stmt* type_enum_stmt =
                    _rd_db_prepare_get_type_enum_params(ctx, tdef->name,
                                                        p_name);
                _rd_db_step(ctx, type_enum_stmt);

                i64 e_value = (i64)sqlite3_column_int64(type_enum_stmt, 0);
                rd_typedef_add_enumval(tdef, p_name, e_value, ctx);
            }
            else if(kind == RD_TKIND_FUNC) {
                if(p_midx > 0) {
                    rd_typedef_add_arg(tdef, p_type, p_name, p_count, p_mod,
                                       ctx);
                }
                else
                    rd_typedef_set_ret(tdef, p_type, p_count, p_mod, ctx);
            }
            else
                unreachable();
        }

        vect_push(v, tdef);
    }

    return v;
}

const char* _rd_i_db_query_get_comment(RDContext* ctx, RDAddress address) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_GET_COMMENT, "\
        SELECT comment \
        FROM Comments \
        WHERE address = :address \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":address", (sqlite3_int64)address);

    if(_rd_db_step(ctx, stmt) == SQLITE_ROW)
        return (const char*)sqlite3_column_text(stmt, 0);

    return NULL;
}

void _rd_i_db_query_set_comment(RDContext* ctx, RDAddress address,
                                const char* cmt) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_SET_COMMENT, "\
        INSERT INTO Comments \
            VALUES (:address, :comment) \
        ON CONFLICT DO  \
            UPDATE SET comment = EXCLUDED.comment \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":address", (sqlite3_int64)address);
    _rd_db_bind_param_str(ctx, stmt, ":comment", cmt);
    _rd_db_step(ctx, stmt);
}

void _rd_i_db_query_del_comment(RDContext* ctx, RDAddress address) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_DEL_COMMENT, "\
        DELETE FROM Comments \
        WHERE address = :address \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":address", (sqlite3_int64)address);
    _rd_db_step(ctx, stmt);
}

void _rd_i_db_query_add_function(RDContext* ctx, const RDFunction* f) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_ADD_FUNCTION, "\
        INSERT INTO Functions \
        VALUES (:address) \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":address", (sqlite3_int64)f->address);
    _rd_db_step(ctx, stmt);
}

void _rd_i_db_query_add_problem(RDContext* ctx, const RDProblem* p) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_ADD_PROBLEM, "\
        INSERT INTO Problems \
        VALUES (:fromaddr, :address, :message) \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":fromaddr",
                          (sqlite3_int64)p->from_address);
    _rd_db_bind_param_int(ctx, stmt, ":address", (sqlite3_int64)p->address);
    _rd_db_bind_param_str(ctx, stmt, ":message", p->message);
    _rd_db_step(ctx, stmt);
}

void _rd_i_db_query_set_sregval(RDContext* ctx, const RDSegmentReg* sreg) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_SET_SREGVAL, "\
        INSERT INTO SegmentRegisters \
            VALUES (:address, :reg, :value, :confidence) \
        ON CONFLICT DO  \
            UPDATE SET value = EXCLUDED.value, \
                       confidence = EXCLUDED.confidence \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":address", (sqlite3_int64)sreg->address);
    _rd_db_bind_param_str(ctx, stmt, ":reg", sreg->name);

    if(sreg->has_value)
        _rd_db_bind_param_int(ctx, stmt, ":value", (sqlite3_int64)sreg->value);
    else
        _rd_db_bind_param_null(ctx, stmt, ":value");

    _rd_db_bind_param_int(ctx, stmt, ":confidence",
                          (sqlite3_int64)sreg->confidence);
    _rd_db_step(ctx, stmt);
}

RDSegmentRegsVect* _rd_i_db_query_get_all_sregval(RDContext* ctx,
                                                  RDSegmentRegsVect* v,
                                                  RDSegmentRegNameVect* names) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_GET_ALL_SREGVAL, "\
        SELECT address, reg, value, confidence \
        FROM SegmentRegisters  \
        ORDER BY address \
    ");

    assert(vect_is_empty(v));
    assert(vect_is_empty(names));

    while(_rd_db_step(ctx, stmt) == SQLITE_ROW) {
        RDAddress address = (RDAddress)sqlite3_column_int64(stmt, 0);
        const char* interned = rd_i_strpool_intern(
            &ctx->strings, (const char*)sqlite3_column_text(stmt, 1));
        assert(interned);

        RDRegValue value = 0;
        bool has_value = sqlite3_column_type(stmt, 2) != SQLITE_NULL;
        if(has_value) value = (RDRegValue)sqlite3_column_int64(stmt, 2);

        RDConfidence c = (RDConfidence)sqlite3_column_int(stmt, 3);

        RDSegmentRegVect* sreg_vect =
            _rd_i_db_segmentregs_get_vect(v, names, interned);

        vect_push(sreg_vect, (RDSegmentReg){
                                 .address = address,
                                 .name = interned,
                                 .value = value,
                                 .has_value = has_value,
                                 .confidence = c,
                             });
    }

    return v;
}

void _rd_i_db_query_set_ovr_operand(RDContext* ctx, RDAddress address,
                                    int idx) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_SET_OVR_OPERAND, "\
        INSERT OR IGNORE \
        INTO OperandOverrides \
        VALUES(:address, :index) \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":address", (sqlite3_int64)address);
    _rd_db_bind_param_int(ctx, stmt, ":index", idx);
    _rd_db_step(ctx, stmt);
}

void _rd_i_db_query_del_ovr_operand(RDContext* ctx, RDAddress address,
                                    int idx) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_DEL_OVR_OPERAND, "\
        DELETE FROM OperandOverrides \
        WHERE address = :address \
        AND idx = :index \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":address", (sqlite3_int64)address);
    _rd_db_bind_param_int(ctx, stmt, ":index", idx);
    _rd_db_step(ctx, stmt);
}

bool _rd_i_db_query_has_ovr_operand(RDContext* ctx, RDAddress address) {
    sqlite3_stmt* stmt =
        _rd_db_prepare_query(ctx, RD_QUERY_HAS_OVR_OPERAND,
                             "SELECT EXISTS(SELECT 1 FROM OperandOverrides \
                                WHERE address = :address)");

    _rd_db_bind_param_int(ctx, stmt, ":address", (sqlite3_int64)address);
    bool r = false;
    if(sqlite3_step(stmt) == SQLITE_ROW) r = sqlite3_column_int(stmt, 0);
    return r;
}

RDOvrOperandVect* _rd_i_db_query_get_all_ovr_operand(RDContext* ctx,
                                                     RDAddress address) {
    sqlite3_stmt* stmt =
        _rd_db_prepare_query(ctx, RD_QUERY_GET_ALL_OVR_OPERAND, "\
            SELECT address, idx \
            FROM OperandOverrides \
            WHERE address = :address \
        ");

    vect_clear(&ctx->ovr_ops_buf);
    _rd_db_bind_param_int(ctx, stmt, ":address", (sqlite3_int64)address);

    while(_rd_db_step(ctx, stmt) == SQLITE_ROW) {
        vect_push(&ctx->ovr_ops_buf,
                  (RDOvrOperand){
                      .address = (RDAddress)sqlite3_column_int64(stmt, 0),
                      .index = (usize)sqlite3_column_int64(stmt, 1),
                  });
    }

    return &ctx->ovr_ops_buf;
}

RDConfidence _rd_i_db_query_get_undefine_confidence(RDContext* ctx,
                                                    RDAddress start,
                                                    RDAddress end) {
    sqlite3_stmt* stmt =
        _rd_db_prepare_query(ctx, RD_QUERY_GET_UNDEFINE_CONFIDENCE, "\
            SELECT MAX(confidence) FROM ( \
                SELECT confidence FROM Types \
                WHERE address >= :start AND address < :end \
                UNION ALL \
                SELECT confidence FROM XRefs \
                WHERE from_address >= :start AND from_address < :end \
            )\
        ");

    _rd_db_bind_param_int(ctx, stmt, ":start", (sqlite3_int64)start);
    _rd_db_bind_param_int(ctx, stmt, ":end", (sqlite3_int64)end);

    if(sqlite3_step(stmt) == SQLITE_ROW &&
       sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
        return (RDConfidence)sqlite3_column_int(stmt, 0);
    }

    return RD_CONFIDENCE_AUTO;
}
