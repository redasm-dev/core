#include "queries.h"
#include "core/context.h"
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

static int _rd_db_step(const RDContext* ctx, sqlite3_stmt* stmt) {
    int res = sqlite3_step(stmt);
    if((res == SQLITE_ROW) || (res == SQLITE_DONE)) return res;
    panic("SQL: %s", sqlite3_errmsg(ctx->db->handle));
}

static sqlite3_stmt* _rd_db_prepare_query(RDContext* ctx, int id,
                                          const char* q) {
    if(!ctx->db->queries[id]) {
        sqlite3_stmt* stmt = NULL;
        int rc = sqlite3_prepare_v2(ctx->db->handle, q, strlen(q), &stmt, NULL);

        if(rc != SQLITE_OK)
            panic("SQL: %s\nQUERY: %s", sqlite3_errmsg(ctx->db->handle), q);

        assert(stmt && "statement is NULL");
        ctx->db->queries[id] = stmt;
    }
    else
        sqlite3_reset(ctx->db->queries[id]);

    return ctx->db->queries[id];
}

static sqlite3_stmt* _rd_db_prepare_set_typedef_params_query(RDContext* ctx) {
    return _rd_db_prepare_query(ctx, RD_QUERY_SET_TYPEDEF_PARAMS, "\
        INSERT INTO TypeParams \
            VALUES (:owner, :type, :name, :count, :modifier, :member_idx) \
        ON CONFLICT DO \
            UPDATE SET type = EXCLUDED.type, \
                       name = EXCLUDED.name, \
                       count = EXCLUDED.count, \
                       modifier = EXCLUDED.modifier, \
                       member_idx = EXCLUDED.member_idx \
        ");
}

static sqlite3_stmt* _rd_db_prepare_get_typedef_params_query(RDContext* ctx) {
    return _rd_db_prepare_query(ctx, RD_QUERY_GET_TYPEDEF_PARAMS, "\
        SELECT type, name, count, modifier, member_idx \
        FROM TypeParams \
        WHERE owner = :owner \
        ORDER BY member_idx \
        ");
}

static int _rd_db_step_params(sqlite3_stmt* stmt, RDContext* ctx, RDParam* p) {
    if(_rd_db_step(ctx, stmt) != SQLITE_ROW) return -1;

    p->type = (RDType){
        .name = rd_i_strpool_intern(&ctx->strings,
                                    (const char*)sqlite3_column_text(stmt, 0)),
        .count = sqlite3_column_int(stmt, 2),
        .mod = (RDTypeModifier)sqlite3_column_int(stmt, 3),

    };

    p->name = rd_i_strpool_intern(&ctx->strings,
                                  (const char*)sqlite3_column_text(stmt, 1));

    return sqlite3_column_int(stmt, 4);
}

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

void _rd_i_db_query_set_info_int(RDContext* ctx, const char* key, u64 val) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_SET_INFO_INT, "\
        INSERT INTO Info(key, int_val) \
            VALUES (:key, :int_val) \
        ON CONFLICT(key) DO \
            UPDATE SET int_val = EXCLUDED.int_val \
    ");

    _rd_db_bind_param_str(ctx, stmt, ":key", key);
    _rd_db_bind_param_int(ctx, stmt, ":int_val", val);
    _rd_db_step(ctx, stmt);
}

bool _rd_i_db_query_get_info_int(RDContext* ctx, const char* key, u64* val) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_GET_INFO_INT, "\
        SELECT int_val \
        FROM Info \
        WHERE key = :key \
        LIMIT 1 \
    ");

    _rd_db_bind_param_str(ctx, stmt, ":key", key);

    if(_rd_db_step(ctx, stmt) == SQLITE_ROW &&
       sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
        if(val) *val = (u64)sqlite3_column_int64(stmt, 0);
        return true;
    }

    return false;
}

void _rd_i_db_query_set_info_str(RDContext* ctx, const char* key,
                                 const char* val) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_SET_INFO_STR, "\
        INSERT INTO Info(key, str_val) \
            VALUES (:key, :str_val) \
        ON CONFLICT(key) DO \
            UPDATE SET str_val = EXCLUDED.str_val \
    ");

    _rd_db_bind_param_str(ctx, stmt, ":key", key);
    _rd_db_bind_param_str(ctx, stmt, ":str_val", val);
    _rd_db_step(ctx, stmt);
}

bool _rd_db_i_query_get_info_str(RDContext* ctx, const char* key,
                                 const char** val) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_GET_INFO_STR, "\
        SELECT str_val \
        FROM Info \
        WHERE key = :key \
        LIMIT 1 \
    ");

    _rd_db_bind_param_str(ctx, stmt, ":key", key);

    if(_rd_db_step(ctx, stmt) == SQLITE_ROW &&
       sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
        if(val) *val = (const char*)sqlite3_column_text(stmt, 0);
        return true;
    }

    return false;
}

void _rd_i_db_query_add_segment(RDContext* ctx, const RDSegmentFull* s) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_ADD_SEGMENT, "\
        INSERT INTO Segments \
        VALUES (:name, :startaddr, :endaddr, :unit, :perm) \
    ");

    _rd_db_bind_param_str(ctx, stmt, ":name", s->base.name);
    _rd_db_bind_param_int(ctx, stmt, ":startaddr", s->base.start_address);
    _rd_db_bind_param_int(ctx, stmt, ":endaddr", s->base.end_address);
    _rd_db_bind_param_int(ctx, stmt, ":unit", s->base.unit);
    _rd_db_bind_param_int(ctx, stmt, ":perm", s->base.perm);
    _rd_db_step(ctx, stmt);
}

void _rd_i_db_query_add_mapping(RDContext* ctx, const RDInputMapping* m) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_ADD_MAPPING, "\
        INSERT INTO InputMappings \
        VALUES (:offset, :startaddr, :endaddr) \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":offset", m->offset);
    _rd_db_bind_param_int(ctx, stmt, ":startaddr", m->start_address);
    _rd_db_bind_param_int(ctx, stmt, ":endaddr", m->end_address);
    _rd_db_step(ctx, stmt);
}

void _rd_i_db_query_set_imported(RDContext* ctx, RDAddress address,
                                 const RDImported* imp) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_SET_IMPORTED, "\
        INSERT INTO Imported \
            VALUES(:address, :ordinal, :module) \
        ON CONFLICT DO \
            UPDATE SET ordinal = EXCLUDED.ordinal, \
                       module  = EXCLUDED.module \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":address", address);

    if(imp->ordinal.has_value)
        _rd_db_bind_param_int(ctx, stmt, ":ordinal", imp->ordinal.value);
    else
        _rd_db_bind_param_null(ctx, stmt, ":ordinal");

    if(imp->module)
        _rd_db_bind_param_str(ctx, stmt, ":module", imp->module);
    else
        _rd_db_bind_param_null(ctx, stmt, ":module");

    _rd_db_step(ctx, stmt);
}

bool _rd_i_db_query_get_imported(RDContext* ctx, RDAddress address,
                                 RDImported* imp) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_GET_IMPORTED, "\
        SELECT module, ordinal \
        FROM Imported \
        WHERE address = :address \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":address", address);

    if(_rd_db_step(ctx, stmt) == SQLITE_ROW) {
        imp->module = rd_i_strpool_intern(&ctx->strings,
                                          (char*)sqlite3_column_text(stmt, 0));

        if(sqlite3_column_type(stmt, 1) != SQLITE_NULL) {
            imp->ordinal.value = sqlite3_column_int64(stmt, 1);
            imp->ordinal.has_value = true;
        }
        else
            imp->ordinal.has_value = false;

        return true;
    }

    return false;
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

    _rd_db_bind_param_int(ctx, stmt, ":fromaddr", from);
    _rd_db_bind_param_int(ctx, stmt, ":toaddr", to);
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

    _rd_db_bind_param_int(ctx, stmt, ":fromaddr", from);
    _rd_db_bind_param_int(ctx, stmt, ":toaddr", to);
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

    _rd_db_bind_param_int(ctx, stmt, ":fromaddr", from);
    _rd_db_bind_param_int(ctx, stmt, ":toaddr", to);

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

    _rd_db_bind_param_int(ctx, stmt, ":fromaddr", from);
    _rd_db_bind_param_int(ctx, stmt, ":type", type);

    vect_clear(refs);

    while(_rd_db_step(ctx, stmt) == SQLITE_ROW) {
        vect_push(refs, (RDXRef){
                            .address = (RDAddress)sqlite3_column_int64(stmt, 0),
                            .type = (usize)sqlite3_column_int64(stmt, 1),
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

    _rd_db_bind_param_int(ctx, stmt, ":toaddr", to);
    _rd_db_bind_param_int(ctx, stmt, ":type", type);

    vect_clear(refs);

    while(_rd_db_step(ctx, stmt) == SQLITE_ROW) {
        vect_push(refs, (RDXRef){
                            .address = (RDAddress)sqlite3_column_int64(stmt, 0),
                            .type = (usize)sqlite3_column_int64(stmt, 1),
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

    _rd_db_bind_param_int(ctx, stmt, ":fromaddr", from);
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

    _rd_db_bind_param_int(ctx, stmt, ":toaddr", to);
    _rd_db_bind_param_int(ctx, stmt, ":confidence", c);
    _rd_db_step(ctx, stmt);

    return sqlite3_changes(ctx->db->handle) > 0;
}

bool _rd_i_db_query_has_xrefs_from(RDContext* ctx, RDAddress address) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(
        ctx, RD_QUERY_HAS_XREFS_FROM, "SELECT EXISTS(SELECT 1 FROM XRefs \
                                WHERE from_address = :address)");

    if(!stmt) return false;
    _rd_db_bind_param_int(ctx, stmt, ":address", address);

    bool r = false;
    if(sqlite3_step(stmt) == SQLITE_ROW) r = sqlite3_column_int(stmt, 0);
    return r;
}

bool _rd_i_db_query_has_xrefs_to(RDContext* ctx, RDAddress address) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(
        ctx, RD_QUERY_HAS_XREFS_TO,
        "SELECT EXISTS(SELECT 1 FROM XRefs WHERE to_address = :address)");

    if(!stmt) return false;
    _rd_db_bind_param_int(ctx, stmt, ":address", address);

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

    _rd_db_bind_param_int(ctx, stmt, ":address", address);

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

    _rd_db_bind_param_int(ctx, stmt, ":address", address);
    _rd_db_bind_param_str(ctx, stmt, ":name", name);
    _rd_db_bind_param_int(ctx, stmt, ":confidence", c);
    _rd_db_step(ctx, stmt);
}

bool _rd_i_db_query_del_name(RDContext* ctx, RDAddress address) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_DEL_NAME, "\
        DELETE FROM Names \
        WHERE address = :address \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":address", address);
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

    _rd_db_bind_param_int(ctx, stmt, ":address", address);
    _rd_db_bind_param_str(ctx, stmt, ":name", name);
    _rd_db_bind_param_int(ctx, stmt, ":count", count);
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

    _rd_db_bind_param_int(ctx, stmt, ":address", address);

    if(_rd_db_step(ctx, stmt) == SQLITE_ROW) {
        t->base.name = rd_i_strpool_intern(&ctx->strings,
                                           (char*)sqlite3_column_text(stmt, 0));
        t->base.count = sqlite3_column_int64(stmt, 1);
        t->base.mod = sqlite3_column_int64(stmt, 2);
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

    _rd_db_bind_param_int(ctx, stmt, ":address", address);
    _rd_db_step(ctx, stmt);

    return sqlite3_changes(ctx->db->handle) > 0;
}

void _rd_i_db_query_set_type_def(RDContext* ctx, const RDTypeDef* tdef) {
    if(tdef->kind == RD_TKIND_PRIM) return;

    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_SET_TYPEDEF, "\
        INSERT INTO TypeDefs \
            VALUES (:name, :kind, :size, :is_noret, :enum_type) \
        ON CONFLICT DO \
            UPDATE SET kind = EXCLUDED.kind, \
                       size = EXCLUDED.size, \
                       is_noret = EXCLUDED.is_noret, \
                       enum_type = EXCLUDED.enum_type \
    ");

    _rd_db_bind_param_str(ctx, stmt, ":name", tdef->name);
    _rd_db_bind_param_int(ctx, stmt, ":kind", tdef->kind);
    _rd_db_bind_param_int(ctx, stmt, ":size", tdef->size);

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
            stmt = _rd_db_prepare_set_typedef_params_query(ctx);
            _rd_db_bind_param_str(ctx, stmt, ":owner", tdef->name);
            _rd_db_bind_param_str(ctx, stmt, ":type", m->type.name);
            _rd_db_bind_param_str(ctx, stmt, ":name", m->name);
            _rd_db_bind_param_int(ctx, stmt, ":count", m->type.count);
            _rd_db_bind_param_int(ctx, stmt, ":modifier", m->type.mod);
            _rd_db_bind_param_int(ctx, stmt, ":member_idx", ++i);
            _rd_db_step(ctx, stmt);
        }
    }
    else if(tdef->kind == RD_TKIND_ENUM) {
        const RDEnumCase* c;
        vect_each(c, &tdef->enum_) {
            stmt = _rd_db_prepare_set_typedef_params_query(ctx);
            _rd_db_bind_param_str(ctx, stmt, ":owner", tdef->name);
            _rd_db_bind_param_str(ctx, stmt, ":name", c->name);
            _rd_db_bind_param_int(ctx, stmt, ":value", c->value);
            _rd_db_step(ctx, stmt);
        }
    }
    else if(tdef->kind == RD_TKIND_FUNC) {
        if(!rd_i_type_is_void(
               &tdef->func_.ret)) { // return type at member_idx=0
            stmt = _rd_db_prepare_set_typedef_params_query(ctx);
            _rd_db_bind_param_str(ctx, stmt, ":owner", tdef->name);
            _rd_db_bind_param_str(ctx, stmt, ":type", tdef->func_.ret.name);
            _rd_db_bind_param_str(ctx, stmt, ":name", ""); // return has no name
            _rd_db_bind_param_int(ctx, stmt, ":count", tdef->func_.ret.count);
            _rd_db_bind_param_int(ctx, stmt, ":modifier", tdef->func_.ret.mod);
            _rd_db_bind_param_int(ctx, stmt, ":member_idx", 0);
            _rd_db_step(ctx, stmt);
        }

        // args type at member_idx=1..n
        const RDParam* p;
        usize i = 1;
        vect_each(p, &tdef->func_.args) {
            stmt = _rd_db_prepare_set_typedef_params_query(ctx);
            _rd_db_bind_param_str(ctx, stmt, ":owner", tdef->name);
            _rd_db_bind_param_str(ctx, stmt, ":type", p->type.name);
            _rd_db_bind_param_str(ctx, stmt, ":name", p->name);
            _rd_db_bind_param_int(ctx, stmt, ":count", p->type.count);
            _rd_db_bind_param_int(ctx, stmt, ":modifier", p->type.mod);
            _rd_db_bind_param_int(ctx, stmt, ":member_idx", i++);
            _rd_db_step(ctx, stmt);
        }
    }
    else
        unreachable();
}

const char* _rd_i_db_query_get_comment(RDContext* ctx, RDAddress address) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_GET_COMMENT, "\
        SELECT comment \
        FROM Comments \
        WHERE address = :address \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":address", address);

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

    _rd_db_bind_param_int(ctx, stmt, ":address", address);
    _rd_db_bind_param_str(ctx, stmt, ":comment", cmt);
    _rd_db_step(ctx, stmt);
}

void _rd_i_db_query_del_comment(RDContext* ctx, RDAddress address) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_DEL_COMMENT, "\
        DELETE FROM Comments \
        WHERE address = :address \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":address", address);
    _rd_db_step(ctx, stmt);
}

void _rd_i_db_query_add_problem(RDContext* ctx, const RDProblem* p) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_ADD_PROBLEM, "\
        INSERT INTO Problems \
            VALUES (:fromaddr, :address, :message) \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":fromaddr", p->from_address);
    _rd_db_bind_param_int(ctx, stmt, ":address", p->address);
    _rd_db_bind_param_str(ctx, stmt, ":message", p->message);
    _rd_db_step(ctx, stmt);
}

bool _rd_i_db_query_get_userdata(RDContext* ctx, const char* key, uptr* ud) {
    assert(ud && "userdata is NULL");
    if(!key || !key[0]) return false;

    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_GET_USERDATA, "\
        SELECT v \
        FROM UserData \
        WHERE k = :k \
    ");

    _rd_db_bind_param_str(ctx, stmt, ":k", key);

    if(_rd_db_step(ctx, stmt) == SQLITE_ROW) {
        *ud = (uptr)sqlite3_column_int64(stmt, 0);
        return true;
    }

    return false;
}

void _rd_i_db_query_set_userdata(RDContext* ctx, const char* key, uptr ud) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_SET_USERDATA, "\
        INSERT INTO UserData \
            VALUES (:k, :v) \
        ON CONFLICT DO  \
            UPDATE SET v = EXCLUDED.v \
    ");

    _rd_db_bind_param_str(ctx, stmt, ":k", key);
    _rd_db_bind_param_int(ctx, stmt, ":v", ud);
    _rd_db_step(ctx, stmt);
}

void _rd_i_db_query_set_regval(RDContext* ctx, RDAddress address,
                               const char* regname, RDRegValue val,
                               RDConfidence c) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_SET_REGVAL, "\
        INSERT INTO SegmentRegisters \
            VALUES (:address, :reg, :value, :confidence) \
        ON CONFLICT DO  \
            UPDATE SET value = EXCLUDED.value, \
                       confidence = EXCLUDED.confidence \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":address", address);
    _rd_db_bind_param_str(ctx, stmt, ":reg", regname);
    _rd_db_bind_param_int(ctx, stmt, ":value", val);
    _rd_db_bind_param_int(ctx, stmt, ":confidence", c);
    _rd_db_step(ctx, stmt);
}

void _rd_i_db_query_set_ovr_operand(RDContext* ctx, RDAddress address,
                                    int idx) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_SET_OVR_OPERAND, "\
            INSERT OR IGNORE \
            INTO OperandOverrides \
            VALUES(:address, :index) \
        ");

    _rd_db_bind_param_int(ctx, stmt, ":address", address);
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

    _rd_db_bind_param_int(ctx, stmt, ":address", address);
    _rd_db_bind_param_int(ctx, stmt, ":index", idx);
    _rd_db_step(ctx, stmt);
}

bool _rd_i_db_query_has_ovr_operand(RDContext* ctx, RDAddress address) {
    sqlite3_stmt* stmt =
        _rd_db_prepare_query(ctx, RD_QUERY_HAS_OVR_OPERAND,
                             "SELECT EXISTS(SELECT 1 FROM OperandOverrides \
                                WHERE address = :address)");

    _rd_db_bind_param_int(ctx, stmt, ":address", address);
    bool r = false;
    if(sqlite3_step(stmt) == SQLITE_ROW) r = sqlite3_column_int(stmt, 0);
    return r;
}

RDOvrOperandVect* _rd_i_db_query_get_all_ovr_operand(RDContext* ctx,
                                                     RDAddress address) {
    sqlite3_stmt* stmt =
        _rd_db_prepare_query(ctx, RD_QUERY_GET_ALL_OVR_OPERAND, "\
            SELECT address, idx FROM OperandOverrides \
            WHERE address = :address \
        ");

    vect_clear(&ctx->ovr_ops_buf);
    _rd_db_bind_param_int(ctx, stmt, ":address", address);

    while(_rd_db_step(ctx, stmt) == SQLITE_ROW) {
        vect_push(&ctx->ovr_ops_buf,
                  (RDOvrOperand){
                      .address = (RDAddress)sqlite3_column_int64(stmt, 0),
                      .index = (int)sqlite3_column_int64(stmt, 1),
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

    _rd_db_bind_param_int(ctx, stmt, ":start", start);
    _rd_db_bind_param_int(ctx, stmt, ":end", end);

    if(sqlite3_step(stmt) == SQLITE_ROW &&
       sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
        return (RDConfidence)sqlite3_column_int(stmt, 0);
    }

    return RD_CONFIDENCE_AUTO;
}
