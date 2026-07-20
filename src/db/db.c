#include "db.h"
#include "core/context.h"
#include "db/schema.h"
#include "db/types.h"
#include "support/containers.h"
#include "support/error.h"
#include "support/utils.h"
#include <redasm/support/logging.h>

#define RD_DB_KEY_ENTRY_POINT "entry_point"

bool rd_i_db_is_valid(const char* dbpath) {
    sqlite3* db = NULL;
    bool valid = false;

    if(sqlite3_open_v2(dbpath, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK)
        goto done;

    sqlite3_stmt* stmt = NULL;
    if(sqlite3_prepare_v2(db, "PRAGMA integrity_check", -1, &stmt, NULL) !=
       SQLITE_OK)
        goto done;

    if(sqlite3_step(stmt) == SQLITE_ROW) {
        const char* result = (const char*)sqlite3_column_text(stmt, 0);
        valid = result && strcmp(result, "ok") == 0;
    }

    sqlite3_finalize(stmt);

done:
    if(db) sqlite3_close(db);
    return valid;
}

RDDB* rd_i_db_create(const char* dbpath) {
    RDDB* self = rd_alloc0(1, sizeof(*self));
    self->filepath = rd_strdup(dbpath);
    assert(self->filepath);

    if(sqlite3_open(self->filepath, &self->handle) != SQLITE_OK) {
        panic("cannot open database at %s", self->filepath);
        goto fail;
    }

    char* errmsg = NULL;
    int rc = sqlite3_exec(self->handle, DB_SCHEMA, NULL, NULL, &errmsg);

    if(rc != SQLITE_OK) {
        panic("sqlite schema error: %s", errmsg);
        goto fail;
    }

    return self;

fail:
    rd_free(self->filepath);
    rd_free(self);
    return NULL;
}

void rd_i_db_destroy(RDDB* self) {
    if(!self) return;

    vect_destroy(&self->segment_reg_names);

    RDSegmentRegVect* v;
    vect_each(v, &self->segment_regs) { vect_destroy(v); }
    vect_destroy(&self->segment_regs);

    RDSegmentFull** s;
    vect_each(s, &self->segments) { rd_i_segment_destroy(*s); }
    vect_destroy(&self->segments);

    vect_destroy(&self->mappings);

    for(int i = 0; i < RD_QUERY_COUNT; i++) {
        if(self->queries[i]) sqlite3_finalize(self->queries[i]);
    }

    sqlite3_close(self->handle);

    if(self->filepath) {
        remove(self->filepath);
        rd_free(self->filepath);
    }

    self->handle = NULL;
    self->filepath = NULL;
    rd_free(self);
}

void rd_i_db_begin(RDContext* ctx) {
    sqlite3_exec(ctx->db->handle, "BEGIN", NULL, NULL, NULL);
}

void rd_i_db_commit(RDContext* ctx) {
    sqlite3_exec(ctx->db->handle, "COMMIT", NULL, NULL, NULL);
}

void rd_i_db_rollback(RDContext* ctx) {
    sqlite3_exec(ctx->db->handle, "ROLLBACK", NULL, NULL, NULL);
}

void rd_i_db_load_segments(RDContext* ctx) {
    _rd_i_db_query_get_all_segments(ctx, &ctx->db->segments);
    _rd_i_db_query_get_all_mappings(ctx, &ctx->db->mappings);
}

void rd_i_db_save(RDContext* ctx) {
    rd_i_db_begin(ctx);

    // clang-format off
    sqlite3_exec(ctx->db->handle, "DELETE FROM Functions", NULL, NULL, NULL);
    sqlite3_exec(ctx->db->handle, "DELETE FROM SegmentRegisters", NULL, NULL, NULL);
    // clang-format on

    RDFunction** f;
    vect_each(f, &ctx->functions) _rd_i_db_query_set_function(ctx, *f);

    RDSegmentRegVect* sreg_vect;
    vect_each(sreg_vect, &ctx->db->segment_regs) {
        RDSegmentReg* sreg;
        vect_each(sreg, sreg_vect) _rd_i_db_query_set_sregval(ctx, sreg);
    }

    rd_i_db_commit(ctx);
}

void rd_i_db_load(RDContext* ctx) {
    _rd_i_db_query_get_all_type_defs(ctx, &ctx->typedefs);

    // restore NORET status in KB
    RDTypeDef** it;
    vect_each(it, &ctx->typedefs) {
        rd_i_typedef_measure(ctx, *it);

        if((*it)->kind == RD_TKIND_FUNC && (*it)->func_.is_noret)
            rd_i_kb_add_noret(ctx, (*it)->name);
    }

    _rd_i_db_query_get_all_functions(ctx, &ctx->functions);
    _rd_i_db_query_get_all_sregval(ctx, &ctx->db->segment_regs,
                                   &ctx->db->segment_reg_names);
}

bool rd_i_db_export(RDContext* ctx, const char* filepath) {
    if(!filepath) return false;

    if(!strcmp(filepath, ctx->db->filepath)) {
        RD_LOG_FAIL("cannot export to the active database path '%s'", filepath);
        return false;
    }

    if(rd_i_file_exists(filepath))
        remove(filepath); // clean db, don't merge an existing one

    sqlite3_backup* backup = NULL;
    sqlite3* exported = NULL;
    bool ok = false;

    if(sqlite3_open(filepath, &exported) != SQLITE_OK) {
        RD_LOG_FAIL("cannot export database at %s", filepath);
        goto done;
    }

    rd_i_db_save(ctx);
    backup = sqlite3_backup_init(exported, "main", ctx->db->handle, "main");
    if(!backup) goto done;

    sqlite3_backup_step(backup, -1);
    sqlite3_backup_finish(backup);

    ok = sqlite3_errcode(exported) == SQLITE_OK;
    if(!ok) RD_LOG_FAIL("SQL: %s", sqlite3_errmsg(exported));

    sqlite3_exec(exported, "VACUUM", NULL, NULL, NULL);

done:
    if(exported) sqlite3_close(exported);
    return ok;
}

bool rd_i_db_add_segment(RDContext* ctx, RDSegmentFull* seg) {
    if(seg->base.start_address >= seg->base.end_address) return false;

    // check for overlaps with existing segments
    RDSegmentFull** it;
    vect_each(it, &ctx->db->segments) {
        RDAddress s = (*it)->base.start_address;
        RDAddress e = (*it)->base.end_address;
        if(seg->base.start_address < e && seg->base.end_address > s) {
            rd_i_add_problem(
                ctx, seg->base.start_address, seg->base.start_address,
                "segment '%s' [%llx, %llx) overlaps with '%s' [%llx, %llx)",
                seg->base.name, seg->base.start_address, seg->base.end_address,
                (*it)->base.name, s, e);
            return false;
        }
    }

    vect_push(&ctx->db->segments, seg);
    vect_sort(&ctx->db->segments, _rd_i_db_segment_cmp_pred);
    _rd_i_db_query_add_segment(ctx, seg);
    return true;
}

bool rd_i_db_find_segment_index(const RDContext* ctx, RDAddress address,
                                usize* index) {
    usize seg_idx =
        vect_bsearch(&ctx->db->segments, &address, _rd_i_db_segment_find_pred);

    if(seg_idx < vect_length(&ctx->db->segments)) {
        if(index) *index = seg_idx;
        return true;
    }

    return false;
}

const RDSegmentFull* rd_i_db_find_segment(const RDContext* ctx,
                                          RDAddress address) {
    if(vect_is_empty(&ctx->db->segments)) return NULL;

    // check if before the first address
    if(address < (*vect_first(&ctx->db->segments))->base.start_address)
        return NULL;

    // check if after the last address
    if(address >= (*vect_last(&ctx->db->segments))->base.end_address)
        return NULL;

    if(ctx->db->last_segment &&
       rd_i_segment_contains(ctx->db->last_segment, address))
        return ctx->db->last_segment;

    usize idx =
        vect_bsearch(&ctx->db->segments, &address, _rd_i_db_segment_find_pred);
    if(idx == vect_length(&ctx->db->segments)) return NULL;

    ctx->db->last_segment = *vect_at(&ctx->db->segments, idx);
    return ctx->db->last_segment;
}

const RDSegmentFullVect* rd_i_db_get_segments(const RDContext* ctx) {
    return &ctx->db->segments;
}

bool rd_i_db_add_mapping(RDContext* ctx, RDInputMapping m) {
    RDAddress endoff = m.end_address - m.start_address + m.offset;
    if(m.offset >= endoff || m.start_address >= m.end_address) return false;

    usize n = m.end_address - m.start_address;
    if(m.offset + n > ctx->input->base.length) return false;

    // segment must exist and mapping must not cross boundary
    const RDSegmentFull* seg = rd_i_db_find_segment(ctx, m.start_address);

    if(!seg) {
        rd_i_add_problem(ctx, m.start_address, m.start_address,
                         "no segment at %llx", m.start_address);
        return false;
    }

    if(m.end_address > seg->base.end_address) {
        rd_i_add_problem(
            ctx, m.start_address, m.start_address,
            "[%llx, %llx) crosses segment boundary '%s' [%llx, %llx)",
            m.start_address, m.end_address, seg->base.name,
            seg->base.start_address, seg->base.end_address);
        return false;
    }

    // overlap check against existing mappings
    RDInputMapping* it;
    vect_each(it, &ctx->db->mappings) {
        if(m.start_address < it->end_address &&
           m.end_address > it->start_address) {
            rd_i_add_problem(
                ctx, m.start_address, m.start_address,
                "[%llx, %llx) overlaps with existing mapping [%llx, %llx)",
                m.start_address, m.end_address, it->start_address,
                it->end_address);
            return false;
        }
    }

    // write bytes into the segment's flags buffer
    usize buf_idx = rd_i_address2index(seg, m.start_address);
    rd_i_buffer_write((RDBuffer*)seg->flags, buf_idx,
                      ctx->input->data + m.offset, n);

    vect_push(&ctx->db->mappings, m);
    vect_sort(&ctx->db->mappings, _rd_i_db_mapping_cmp_pred);
    _rd_i_db_query_add_mapping(ctx, &m);
    return true;
}

const RDMappingVect* rd_i_db_get_mappings(const RDContext* ctx) {
    return &ctx->db->mappings;
}

void rd_i_db_set_external(RDContext* ctx, const RDExternal* exp) {
    _rd_i_db_query_set_external(ctx, exp);
}

bool rd_i_db_get_external(RDContext* ctx, RDAddress address, RDExternal* ext) {
    return _rd_i_db_query_get_external(ctx, address, ext);
}

bool rd_i_db_get_external_ord(RDContext* ctx, const char* module, u32 ord,
                              RDExternalKind kind, RDExternal* ext) {
    return _rd_i_db_query_get_external_ord(ctx, module, ord, kind, ext);
}

RDExternalVect* rd_i_db_get_all_externals(RDContext* ctx, RDExternalKind kind,
                                          RDExternalVect* v) {
    return _rd_i_db_query_get_all_externals(ctx, kind, v);
}

void rd_i_db_add_xref(RDContext* ctx, RDAddress from, RDAddress to,
                      RDXRefType type, RDConfidence c) {
    _rd_i_db_query_add_xref(ctx, from, to, type, c);
}

bool rd_i_db_del_xref(RDContext* ctx, RDAddress from, RDAddress to,
                      RDConfidence c) {
    return _rd_i_db_query_del_xref(ctx, from, to, c);
}

bool rd_i_db_get_xref(RDContext* ctx, RDAddress from, RDAddress to,
                      RDXRefFull* xref) {
    return _rd_i_db_query_get_xref(ctx, from, to, xref);
}

RDXRefVect* rd_i_db_get_xrefs_from(RDContext* ctx, RDAddress from,
                                   RDXRefType type, RDXRefVect* refs) {
    return _rd_i_db_query_get_xrefs_from(ctx, from, type, refs);
}

RDXRefVect* rd_i_db_get_xrefs_to(RDContext* ctx, RDAddress to, RDXRefType type,
                                 RDXRefVect* refs) {
    return _rd_i_db_query_get_xrefs_to(ctx, to, type, refs);
}

bool rd_i_db_del_xrefs_from(RDContext* ctx, RDAddress from, RDConfidence c) {
    return _rd_i_db_query_del_xrefs_from(ctx, from, c);
}

bool rd_i_db_del_xrefs_to(RDContext* ctx, RDAddress to, RDConfidence c) {
    return _rd_i_db_query_del_xrefs_to(ctx, to, c);
}

bool rd_i_db_has_xrefs_from(RDContext* ctx, RDAddress address) {
    return _rd_i_db_query_has_xrefs_from(ctx, address);
}

bool rd_i_db_has_xrefs_to(RDContext* ctx, RDAddress address) {
    return _rd_i_db_query_has_xrefs_to(ctx, address);
}

bool rd_i_db_get_address(RDContext* ctx, const char* name, RDAddress* address) {
    return _rd_i_db_query_get_address(ctx, name, address);
}

bool rd_i_db_get_name(RDContext* ctx, RDAddress address, RDName* n) {
    return _rd_i_db_query_get_name(ctx, address, n);
}

void rd_i_db_set_name(RDContext* ctx, RDAddress address, const char* name,
                      RDConfidence c) {
    _rd_i_db_query_set_name(ctx, address, name, c);
}

bool rd_i_db_del_name(RDContext* ctx, RDAddress address) {
    return _rd_i_db_query_del_name(ctx, address);
}

RDNameVect* rd_i_db_get_all_names(RDContext* ctx, RDAddressVect* av,
                                  RDNameVect* v) {
    return _rd_i_db_query_get_all_names(ctx, av, v);
}

RDAddressVect* rd_i_db_get_all_name_addresses(RDContext* ctx,
                                              RDAddressVect* v) {
    return _rd_i_db_query_get_all_name_addresses(ctx, v);
}

void rd_i_db_set_type_def(RDContext* ctx, const RDTypeDef* tdef) {
    _rd_i_db_query_set_type_def(ctx, tdef);
}

void rd_i_db_set_type(RDContext* ctx, RDAddress address, const RDType* t,
                      RDConfidence c) {
    _rd_i_db_query_set_type(ctx, address, t, c);
}

bool rd_i_db_get_type(RDContext* ctx, RDAddress address, RDTypeFull* t) {
    return _rd_i_db_query_get_type(ctx, address, t);
}

bool rd_i_db_get_root_type(RDContext* ctx, RDAddress* address, RDType* t) {
    return _rd_i_db_query_get_root_type(ctx, address, t);
}

bool rd_i_db_del_type(RDContext* ctx, RDAddress address) {
    return _rd_i_db_query_del_type(ctx, address);
}

void rd_i_db_set_function(RDContext* ctx, const RDFunction* f) {
    _rd_i_db_query_set_function(ctx, f);
}

RDAddressVect* rd_i_db_get_all_address_by_type(RDContext* ctx, RDAddressVect* v,
                                               const char* filter) {
    return _rd_i_db_query_get_address_by_type(ctx, v, filter);
}

RDTypeVect* rd_i_db_get_all_types(RDContext* ctx, RDAddressVect* av,
                                  RDTypeVect* v) {
    return _rd_i_db_query_get_all_types(ctx, av, v);
}

const char* rd_i_db_get_comment(RDContext* ctx, RDAddress address) {
    return _rd_i_db_query_get_comment(ctx, address);
}

void rd_i_db_set_comment(RDContext* ctx, RDAddress address, const char* cmt) {
    _rd_i_db_query_set_comment(ctx, address, cmt);
}

void rd_i_db_del_comment(RDContext* ctx, RDAddress address) {
    _rd_i_db_query_del_comment(ctx, address);
}

void rd_i_db_add_problem(RDContext* ctx, const RDProblem* p) {
    _rd_i_db_query_add_problem(ctx, p);
}

bool rd_i_db_set_sregval(RDContext* ctx, RDAddress address, const char* regname,
                         RDRegValue val, RDConfidence c) {
    const char* interned = rd_i_strpool_intern(&ctx->strings, regname);
    RDSegmentRegVect* rv = _rd_i_db_segmentregs_get_vect(
        &ctx->db->segment_regs, &ctx->db->segment_reg_names, interned);

    RDSegmentReg entry = {
        .address = address,
        .name = interned,
        .value = val,
        .has_value = true,
        .confidence = c,
    };

    RDSegmentReg key = {.address = address};
    usize idx = vect_lower_bound(rv, &key, _rd_i_db_segmentreg_cmp);

    if(idx < vect_length(rv) && vect_at(rv, idx)->address == address) {
        if(c < vect_at(rv, idx)->confidence) return false;
        *vect_at(rv, idx) = entry;
        return true;
    }

    vect_ins(rv, idx, entry);
    return true;
}

bool rd_i_db_del_sregval(RDContext* ctx, RDAddress address, const char* regname,
                         RDConfidence c) {
    const char* interned = rd_i_strpool_intern(&ctx->strings, regname);
    RDSegmentRegVect* rv =
        _rd_i_db_segmentregs_find_vect(&ctx->db->segment_regs, interned);
    if(!rv) return false;

    RDSegmentReg entry = {
        .address = address,
        .name = interned,
        .value = 0,
        .has_value = false,
        .confidence = c,
    };

    RDSegmentReg key = {.address = address};
    usize idx = vect_lower_bound(rv, &key, _rd_i_db_segmentreg_cmp);

    if(idx < vect_length(rv) && vect_at(rv, idx)->address == address) {
        if(c < vect_at(rv, idx)->confidence) return false;
        *vect_at(rv, idx) = entry;
        return true;
    }

    vect_ins(rv, idx, entry);
    return true;
}

bool rd_i_db_get_sregval(RDContext* ctx, RDAddress address, const char* regname,
                         RDRegValue* value) {
    const char* interned = rd_i_strpool_intern(&ctx->strings, regname);
    RDSegmentRegVect* rv =
        _rd_i_db_segmentregs_find_vect(&ctx->db->segment_regs, interned);
    if(!rv || vect_is_empty(rv)) return false;

    // upper_bound then step back: largest address <= query_address
    RDSegmentReg key = {.address = address};
    usize idx = vect_upper_bound(rv, &key, _rd_i_db_segmentreg_cmp);
    if(!idx) return false;

    const RDSegmentReg* e = &rv->data[idx - 1];
    if(!e->has_value) return false;
    if(value) *value = e->value;
    return true;
}

const RDSegmentRegNameVect* rd_i_db_get_all_sreg_names(const RDContext* ctx) {
    return &ctx->db->segment_reg_names;
}

const RDSegmentRegVect* rd_i_db_get_all_sregval(RDContext* ctx,
                                                const char* regname) {
    if(!regname) return NULL;

    const char* interned = rd_i_strpool_intern(&ctx->strings, regname);
    return _rd_i_db_segmentregs_find_vect(&ctx->db->segment_regs, interned);
}

void rd_i_db_set_ovr_operand(RDContext* ctx, RDAddress address, int idx) {
    _rd_i_db_query_set_ovr_operand(ctx, address, idx);
}

void rd_i_db_del_ovr_operand(RDContext* ctx, RDAddress address, int idx) {
    _rd_i_db_query_del_ovr_operand(ctx, address, idx);
}

bool rd_i_db_has_ovr_operand(RDContext* ctx, RDAddress address) {
    return _rd_i_db_query_has_ovr_operand(ctx, address);
}

RDOvrOperandVect* rd_i_db_get_all_ovr_operand(RDContext* ctx,
                                              RDAddress address) {
    return _rd_i_db_query_get_all_ovr_operand(ctx, address);
}

RDConfidence rd_i_db_get_undefine_confidence(RDContext* ctx, RDAddress start,
                                             RDAddress end) {
    return _rd_i_db_query_get_undefine_confidence(ctx, start, end);
}
