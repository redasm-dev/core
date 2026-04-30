#include "db.h"
#include "core/context.h"
#include "core/db/schema.h"
#include "support/containers.h"
#include "support/error.h"
#include "support/stringpool.h"
#include "support/utils.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RD_DB_KEY_ENTRY_POINT "entry_point"

#define RD_DB_BIND(ctx, stmt, id, bind_call)                                   \
    do {                                                                       \
        int _idx = sqlite3_bind_parameter_index((stmt), (id));                 \
        panic_if(!_idx, "SQL: parameter '%s' not found", (id));                \
        int _res = (bind_call);                                                \
        panic_if(_res != SQLITE_OK, "SQL: %s",                                 \
                 sqlite3_errmsg((ctx)->db.handle));                            \
    } while(0)

static char* _rd_db_get_dbpath(RDContext* ctx) {
    if(!ctx->filepath) return NULL;

    char* filestem = rd_i_get_file_stem(ctx->filepath);
    int filestem_len = strlen(filestem);
    int id_len = strlen(ctx->loaderplugin->id);

    int n = filestem_len + id_len + 5;
    char* dbname = malloc(n);
    snprintf(dbname, n, "%s_%s.db", filestem, ctx->loaderplugin->id);

    char* dbpath = rd_i_get_unique_temp_path(dbname);
    free(dbname);
    free(filestem);
    return dbpath;
}

static sqlite3_stmt* _rd_db_prepare_query(RDContext* ctx, int id,
                                          const char* q) {
    if(!ctx->db.queries[id]) {
        sqlite3_stmt* stmt = NULL;
        int rc = sqlite3_prepare_v2(ctx->db.handle, q, strlen(q), &stmt, NULL);

        if(rc != SQLITE_OK)
            panic("SQL: %s\nQUERY: %s", sqlite3_errmsg(ctx->db.handle), q);

        assert(stmt && "statement is NULL");
        ctx->db.queries[id] = stmt;
    }
    else
        sqlite3_reset(ctx->db.queries[id]);

    return ctx->db.queries[id];
}

static int _rd_db_step(RDContext* ctx, sqlite3_stmt* stmt) {
    int res = sqlite3_step(stmt);
    if((res == SQLITE_ROW) || (res == SQLITE_DONE)) return res;
    panic("SQL: %s", sqlite3_errmsg(ctx->db.handle));
}

static void _rd_db_bind_param_null(RDContext* ctx, sqlite3_stmt* stmt,
                                   const char* id) {
    RD_DB_BIND(ctx, stmt, id, sqlite3_bind_null(stmt, _idx));
}

static void _rd_db_bind_param_int(RDContext* ctx, sqlite3_stmt* stmt,
                                  const char* id, sqlite3_int64 v) {
    RD_DB_BIND(ctx, stmt, id, sqlite3_bind_int64(stmt, _idx, v));
}

static void _rd_db_bind_param_str(RDContext* ctx, sqlite3_stmt* stmt,
                                  const char* id, const char* s) {
    RD_DB_BIND(ctx, stmt, id,
               sqlite3_bind_text(stmt, _idx, s, -1, SQLITE_STATIC));
}

static sqlite3_stmt* _rd_db_prepare_typedef_params_query(RDContext* ctx) {
    return _rd_db_prepare_query(ctx, RD_QUERY_SET_TYPEDEF_PARAMS, "\
        INSERT INTO TypeParams \
            VALUES (:owner, :type, :name, :count, :flags, :member_idx) \
        ON CONFLICT DO \
            UPDATE SET type = EXCLUDED.type, \
                       name = EXCLUDED.name, \
                       count = EXCLUDED.count, \
                       flags = EXCLUDED.flags, \
                       member_idx = EXCLUDED.member_idx \
        ");
}

static void _rd_db_set_info_int(RDContext* ctx, const char* key, u64 val) {
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

static bool _rd_db_get_info_int(RDContext* ctx, const char* key, u64* val) {
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

static void _rd_db_set_info_str(RDContext* ctx, const char* key,
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

static bool _rd_db_get_info_str(RDContext* ctx, const char* key,
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

bool rd_i_db_init(RDContext* ctx) {
    ctx->db.filepath = _rd_db_get_dbpath(ctx);

    if(ctx->db.filepath) // Remove old database (if exists)
        remove(ctx->db.filepath);

    // create an in memory DB if path is not set
    const char* dbpath = ctx->db.filepath ? ctx->db.filepath : ":memory:";

    if(sqlite3_open(dbpath, &ctx->db.handle) != SQLITE_OK) {
        panic("Cannot open database at %s", dbpath);
        goto fail;
    }

    char* errmsg = NULL;
    int rc = sqlite3_exec(ctx->db.handle, DB_SCHEMA, NULL, NULL, &errmsg);

    if(rc != SQLITE_OK) {
        panic("sqlite schema error: %s", errmsg);
        goto fail;
    }

    return true;

fail:
    free(ctx->db.filepath);
    return false;
}

void rd_i_db_deinit(RDContext* ctx) {
    for(int i = 0; i < RD_QUERY_COUNT; i++) {
        if(ctx->db.queries[i]) sqlite3_finalize(ctx->db.queries[i]);
    }

    sqlite3_close(ctx->db.handle);

    if(ctx->db.filepath) {
        remove(ctx->db.filepath);
        free(ctx->db.filepath);
    }

    ctx->db.handle = NULL;
    ctx->db.filepath = NULL;
}

void rd_i_db_add_segment(RDContext* ctx, const RDSegmentFull* s) {
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

void rd_i_db_add_mapping(RDContext* ctx, const RDInputMapping* m) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_ADD_MAPPING, "\
        INSERT INTO InputMappings \
        VALUES (:offset, :startaddr, :endaddr) \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":offset", m->offset);
    _rd_db_bind_param_int(ctx, stmt, ":startaddr", m->start_address);
    _rd_db_bind_param_int(ctx, stmt, ":endaddr", m->end_address);
    _rd_db_step(ctx, stmt);
}

void rd_i_db_set_entry_point(RDContext* ctx, RDAddress address) {
    _rd_db_set_info_int(ctx, RD_DB_KEY_ENTRY_POINT, address);
}

bool rd_i_db_get_entry_point(RDContext* ctx, RDAddress* address) {
    return _rd_db_get_info_int(ctx, RD_DB_KEY_ENTRY_POINT, address);
}

void rd_i_db_set_imported(RDContext* ctx, RDAddress address,
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

bool rd_i_db_get_imported(RDContext* ctx, RDAddress address, RDImported* imp) {
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

void rd_i_db_add_xref(RDContext* ctx, RDAddress from, RDAddress to,
                      usize type) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_ADD_XREF, "\
        INSERT INTO XRefs \
                VALUES (:fromaddr, :toaddr, :type) \
            ON CONFLICT DO \
                UPDATE SET type = EXCLUDED.type \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":fromaddr", from);
    _rd_db_bind_param_int(ctx, stmt, ":toaddr", to);
    _rd_db_bind_param_int(ctx, stmt, ":type", type);
    _rd_db_step(ctx, stmt);
}

void rd_i_db_get_xrefs_from_type(RDContext* ctx, RDAddress from, usize t,
                                 RDXRefVect* refs) {
    sqlite3_stmt* stmt =
        _rd_db_prepare_query(ctx, RD_QUERY_GET_XREFS_FROM_TYPE, "\
            SELECT to_address, type  \
            FROM XRefs \
            WHERE from_address = :fromaddr AND type = :type \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":fromaddr", from);
    _rd_db_bind_param_int(ctx, stmt, ":type", t);

    while(_rd_db_step(ctx, stmt) == SQLITE_ROW) {
        vect_push(refs, (RDXRef){
                            .address = (RDAddress)sqlite3_column_int64(stmt, 0),
                            .type = (usize)sqlite3_column_int64(stmt, 1),
                        });
    }
}

void rd_i_db_get_xrefs_from(RDContext* ctx, RDAddress from, RDXRefVect* refs) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_GET_XREFS_FROM, "\
        SELECT to_address, type  \
        FROM XRefs \
        WHERE from_address = :fromaddr \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":fromaddr", from);

    while(_rd_db_step(ctx, stmt) == SQLITE_ROW) {
        vect_push(refs, (RDXRef){
                            .address = (RDAddress)sqlite3_column_int64(stmt, 0),
                            .type = (usize)sqlite3_column_int64(stmt, 1),
                        });
    }
}

void rd_i_db_get_xrefs_to_type(RDContext* ctx, RDAddress to, usize t,
                               RDXRefVect* refs) {
    sqlite3_stmt* stmt =
        _rd_db_prepare_query(ctx, RD_QUERY_GET_XREFS_TO_TYPE, "\
            SELECT from_address, type  \
            FROM XRefs \
            WHERE to_address = :toaddr AND type = :type \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":toaddr", to);
    _rd_db_bind_param_int(ctx, stmt, ":type", t);

    while(_rd_db_step(ctx, stmt) == SQLITE_ROW) {
        vect_push(refs, (RDXRef){
                            .address = (RDAddress)sqlite3_column_int64(stmt, 0),
                            .type = (usize)sqlite3_column_int64(stmt, 1),
                        });
    }
}

void rd_i_db_get_xrefs_to(RDContext* ctx, RDAddress to, RDXRefVect* refs) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_GET_XREFS_TO, "\
        SELECT from_address, type  \
        FROM XRefs \
        WHERE to_address = :toaddr \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":toaddr", to);

    while(_rd_db_step(ctx, stmt) == SQLITE_ROW) {
        vect_push(refs, (RDXRef){
                            .address = (RDAddress)sqlite3_column_int64(stmt, 0),
                            .type = (usize)sqlite3_column_int64(stmt, 1),
                        });
    }
}

bool rd_i_db_get_address(RDContext* ctx, const char* name, RDAddress* address) {
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

bool rd_i_db_get_name(RDContext* ctx, RDAddress address, RDName* n) {
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

void rd_i_db_set_name(RDContext* ctx, RDAddress address, const char* name,
                      RDConfidence c) {
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

void rd_i_db_del_name(RDContext* ctx, RDAddress address) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_DEL_NAME, "\
        DELETE FROM Names \
        WHERE address = :address \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":address", address);
    _rd_db_step(ctx, stmt);
}

void rd_i_db_set_type_def(RDContext* ctx, const RDTypeDef* tdef) {
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
            stmt = _rd_db_prepare_query(ctx, RD_QUERY_SET_TYPEDEF_PARAMS, "\
                    INSERT INTO TypeParams \
                        VALUES (:owner, :type, :name, :count, :flags, :member_idx) \
                    ON CONFLICT DO \
                        UPDATE SET type = EXCLUDED.type, \
                                   count = EXCLUDED.count, \
                                   flags = EXCLUDED.flags, \
                                   member_idx = EXCLUDED.member_idx \
                    ");

            _rd_db_bind_param_str(ctx, stmt, ":owner", tdef->name);
            _rd_db_bind_param_str(ctx, stmt, ":type", m->type.name);
            _rd_db_bind_param_str(ctx, stmt, ":name", m->name);
            _rd_db_bind_param_int(ctx, stmt, ":count", m->type.count);
            _rd_db_bind_param_int(ctx, stmt, ":flags", m->type.flags);
            _rd_db_bind_param_int(ctx, stmt, ":member_idx", ++i);
            _rd_db_step(ctx, stmt);
        }
    }
    else if(tdef->kind == RD_TKIND_ENUM) {
        const RDEnumCase* c;
        vect_each(c, &tdef->enum_) {
            stmt = _rd_db_prepare_typedef_params_query(ctx);
            _rd_db_bind_param_str(ctx, stmt, ":owner", tdef->name);
            _rd_db_bind_param_str(ctx, stmt, ":name", c->name);
            _rd_db_bind_param_int(ctx, stmt, ":value", c->value);
            _rd_db_step(ctx, stmt);
        }
    }
    else if(tdef->kind == RD_TKIND_FUNC) {
        if(!rd_i_is_void(&tdef->func_.ret)) { // return type at member_idx=0
            stmt = _rd_db_prepare_typedef_params_query(ctx);
            _rd_db_bind_param_str(ctx, stmt, ":owner", tdef->name);
            _rd_db_bind_param_str(ctx, stmt, ":type", tdef->func_.ret.name);
            _rd_db_bind_param_str(ctx, stmt, ":name", ""); // return has no name
            _rd_db_bind_param_int(ctx, stmt, ":count", tdef->func_.ret.count);
            _rd_db_bind_param_int(ctx, stmt, ":flags", tdef->func_.ret.flags);
            _rd_db_bind_param_int(ctx, stmt, ":member_idx", 0);
            _rd_db_step(ctx, stmt);
        }

        // args type at member_idx=1..n
        const RDParam* p;
        usize i = 1;
        vect_each(p, &tdef->func_.args) {
            stmt = _rd_db_prepare_typedef_params_query(ctx);
            _rd_db_bind_param_str(ctx, stmt, ":owner", tdef->name);
            _rd_db_bind_param_str(ctx, stmt, ":type", p->type.name);
            _rd_db_bind_param_str(ctx, stmt, ":name", p->name);
            _rd_db_bind_param_int(ctx, stmt, ":count", p->type.count);
            _rd_db_bind_param_int(ctx, stmt, ":flags", p->type.flags);
            _rd_db_bind_param_int(ctx, stmt, ":member_idx", i++);
            _rd_db_step(ctx, stmt);
        }
    }
    else
        unreachable();
}

void rd_i_db_set_type(RDContext* ctx, RDAddress address, const char* name,
                      usize count, RDTypeFlags flags, RDConfidence c) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_SET_TYPE, "\
        INSERT INTO Types \
            VALUES (:address, :name, :count, :flags, :confidence) \
        ON CONFLICT DO  \
            UPDATE SET name = EXCLUDED.name,  \
                       count = EXCLUDED.count, \
                       flags = EXCLUDED.flags, \
                       confidence = EXCLUDED.confidence \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":address", address);
    _rd_db_bind_param_str(ctx, stmt, ":name", name);
    _rd_db_bind_param_int(ctx, stmt, ":count", count);
    _rd_db_bind_param_int(ctx, stmt, ":flags", flags);
    _rd_db_bind_param_int(ctx, stmt, ":confidence", c);
    _rd_db_step(ctx, stmt);
}

bool rd_i_db_get_type(RDContext* ctx, RDAddress address, RDTypeFull* t) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_GET_TYPE, "\
        SELECT name, count, flags, confidence  \
        FROM Types \
        WHERE address = :address \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":address", address);

    if(_rd_db_step(ctx, stmt) == SQLITE_ROW) {
        t->base.name = rd_i_strpool_intern(&ctx->strings,
                                           (char*)sqlite3_column_text(stmt, 0));
        t->base.count = sqlite3_column_int64(stmt, 1);
        t->base.flags = sqlite3_column_int64(stmt, 2);
        t->confidence = (RDConfidence)sqlite3_column_int64(stmt, 3);
        return true;
    }

    return false;
}

void rd_i_db_del_type(RDContext* ctx, RDAddress address) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_DEL_TYPE, "\
        DELETE FROM Types \
            WHERE address = :address \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":address", address);
    _rd_db_step(ctx, stmt);
}

void rd_i_db_del_type_range(RDContext* ctx, RDAddress startaddr,
                            RDAddress endaddr) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_DEL_TYPE_RANGE, "\
        DELETE FROM Types \
            WHERE address >= :start AND address < :end \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":start", startaddr);
    _rd_db_bind_param_int(ctx, stmt, ":end", endaddr);
    _rd_db_step(ctx, stmt);
}

const char* rd_i_db_get_comment(RDContext* ctx, RDAddress address) {
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

void rd_i_db_set_comment(RDContext* ctx, RDAddress address, const char* cmt) {
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

void rd_i_db_del_comment(RDContext* ctx, RDAddress address) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_DEL_COMMENT, "\
        DELETE FROM Comments \
        WHERE address = :address \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":address", address);
    _rd_db_step(ctx, stmt);
}

void rd_i_db_add_problem(RDContext* ctx, const RDProblem* p) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_ADD_PROBLEM, "\
        INSERT INTO Problems \
            VALUES (:fromaddr, :address, :message) \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":fromaddr", p->from_address);
    _rd_db_bind_param_int(ctx, stmt, ":address", p->address);
    _rd_db_bind_param_str(ctx, stmt, ":message", p->message);
    _rd_db_step(ctx, stmt);
}

bool rd_i_db_get_userdata(RDContext* ctx, const char* key, uptr* ud) {
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

void rd_i_db_set_userdata(RDContext* ctx, const char* key, uptr ud) {
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

void rd_i_db_set_regval(RDContext* ctx, RDAddress address, int reg, u64 val,
                        RDConfidence c) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_SET_REGVAL, "\
        INSERT INTO TrackedRegisters \
            VALUES (:address, :reg, :value, :confidence) \
        ON CONFLICT DO  \
            UPDATE SET value = EXCLUDED.value, \
                       confidence = EXCLUDED.confidence \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":address", address);
    _rd_db_bind_param_int(ctx, stmt, ":reg", reg);
    _rd_db_bind_param_int(ctx, stmt, ":value", val);
    _rd_db_bind_param_int(ctx, stmt, ":confidence", c);
    _rd_db_step(ctx, stmt);
}

bool rd_i_db_get_regval(RDContext* ctx, RDAddress address, int reg,
                        RDRegisterValue* r) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_GET_REGVAL, "\
        SELECT value, confidence FROM TrackedRegisters \
        WHERE reg = :reg AND address <= :address \
        ORDER BY address DESC LIMIT 1 \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":address", address);
    _rd_db_bind_param_int(ctx, stmt, ":reg", reg);

    if(_rd_db_step(ctx, stmt) == SQLITE_ROW) {
        if(r) {
            r->value = (u64)sqlite3_column_int64(stmt, 0);
            r->confidence = (RDConfidence)sqlite3_column_int64(stmt, 1);
        }

        return true;
    }

    return false;
}

bool rd_i_db_get_regval_exact(RDContext* ctx, RDAddress address, int reg,
                              RDRegisterValue* r) {
    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_GET_REGVAL_EXACT, "\
        SELECT value, confidence FROM TrackedRegisters \
        WHERE address = :address AND reg = :reg \
    ");

    _rd_db_bind_param_int(ctx, stmt, ":address", address);
    _rd_db_bind_param_int(ctx, stmt, ":reg", reg);

    if(_rd_db_step(ctx, stmt) == SQLITE_ROW) {
        if(r) {
            r->value = (u64)sqlite3_column_int64(stmt, 0);
            r->confidence = (RDConfidence)sqlite3_column_int64(stmt, 1);
        }

        return true;
    }

    return false;
}

RDTrackedRegisterVect* rd_i_db_get_reg_all(RDContext* ctx,
                                           RDTrackedRegisterVect* regs) {
    vect_clear(regs);

    sqlite3_stmt* stmt = _rd_db_prepare_query(ctx, RD_QUERY_GET_REG_ALL, "\
        SELECT address, reg, value \
        FROM TrackedRegisters \
        ORDER BY address ASC, reg ASC \
    ");

    while(_rd_db_step(ctx, stmt) == SQLITE_ROW) {
        vect_push(regs, (RDTrackedRegister){
                            .address = (RDAddress)sqlite3_column_int64(stmt, 0),
                            .reg = (int)sqlite3_column_int64(stmt, 1),
                            .value = (u64)sqlite3_column_int64(stmt, 2),
                        });
    }

    return regs;
}
