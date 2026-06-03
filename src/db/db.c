#include "db.h"
#include "core/context.h"
#include "db/schema.h"
#include "support/containers.h"
#include "support/error.h"
#include "support/utils.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define RD_DB_KEY_ENTRY_POINT "entry_point"

static char* _rd_db_get_dbpath(const RDContext* ctx) {
    if(!ctx->filepath) return NULL;

    char* filestem = rd_i_get_file_stem(ctx->filepath);
    int filestem_len = (int)strlen(filestem);
    int id_len = (int)strlen(ctx->loaderplugin->id);

    int n = filestem_len + id_len + 5;
    char* dbname = rd_alloc((usize)n);
    snprintf(dbname, (usize)n, "%s_%s.db", filestem, ctx->loaderplugin->id);

    char* dbpath = rd_i_get_unique_temp_path(dbname);
    rd_free(dbname);
    rd_free(filestem);
    return dbpath;
}

static RDSegmentRegVect* _rd_db_segmentregs_find_vect(RDSegmentRegsVect* self,
                                                      const char* reg) {
    RDSegmentRegVect* rv;

    vect_each(rv, self) {
        if(rv->name == reg) return rv;
    }

    return NULL;
}

static RDSegmentRegVect* _rd_db_segmentregs_get_vect(RDDB* db,
                                                     const char* reg) {
    RDSegmentRegVect* rv = _rd_db_segmentregs_find_vect(&db->segment_regs, reg);
    if(rv) return rv;

    // new register, add to both vects
    vect_push(&db->segment_regs, (RDSegmentRegVect){.name = reg});
    vect_push(&db->segment_reg_names, reg);
    return vect_last(&db->segment_regs);
}

RDDB* rd_i_db_create(const RDContext* ctx) {
    RDDB* self = rd_alloc0(1, sizeof(*self));
    self->filepath = _rd_db_get_dbpath(ctx);

    if(self->filepath) // Remove old database (if exists)
        remove(self->filepath);

    // create an in memory DB if path is not set
    const char* dbpath = self->filepath ? self->filepath : ":memory:";

    if(sqlite3_open(dbpath, &self->handle) != SQLITE_OK) {
        panic("Cannot open database at %s", dbpath);
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

void rd_i_db_flush(RDContext* ctx) {}

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
    return true;
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

const RDSegmentVect* rd_i_db_get_segments(const RDContext* ctx) {
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
    return true;
}

const RDMappingVect* rd_i_db_get_mappings(const RDContext* ctx) {
    return &ctx->db->mappings;
}

void rd_i_db_set_entry_point(RDContext* ctx, RDAddress address) {
    _rd_i_db_query_set_info_int(ctx, RD_DB_KEY_ENTRY_POINT, address);
}

bool rd_i_db_get_entry_point(RDContext* ctx, RDAddress* address) {
    return _rd_i_db_query_get_info_int(ctx, RD_DB_KEY_ENTRY_POINT, address);
}

void rd_i_db_set_imported(RDContext* ctx, RDAddress address,
                          const RDImported* imp) {
    _rd_i_db_query_set_imported(ctx, address, imp);
}

bool rd_i_db_get_imported(RDContext* ctx, RDAddress address, RDImported* imp) {
    return _rd_i_db_query_get_imported(ctx, address, imp);
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

void rd_i_db_set_type_def(RDContext* ctx, const RDTypeDef* tdef) {
    _rd_i_db_query_set_type_def(ctx, tdef);
}

void rd_i_db_set_type(RDContext* ctx, RDAddress address, const char* name,
                      usize count, RDTypeModifier mod, RDConfidence c) {
    _rd_i_db_query_set_type(ctx, address, name, count, mod, c);
}

bool rd_i_db_get_type(RDContext* ctx, RDAddress address, RDTypeFull* t) {
    return _rd_i_db_query_get_type(ctx, address, t);
}

bool rd_i_db_del_type(RDContext* ctx, RDAddress address) {
    return _rd_i_db_query_del_type(ctx, address);
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

bool rd_i_db_get_userdata(RDContext* ctx, const char* key, uptr* ud) {
    return _rd_i_db_query_get_userdata(ctx, key, ud);
}

void rd_i_db_set_userdata(RDContext* ctx, const char* key, uptr ud) {
    _rd_i_db_query_set_userdata(ctx, key, ud);
}

bool rd_i_db_set_sregval(RDContext* ctx, RDAddress address, const char* regname,
                         RDRegValue val, RDConfidence c) {
    const char* interned = rd_i_strpool_intern(&ctx->strings, regname);
    RDSegmentRegVect* rv = _rd_db_segmentregs_get_vect(ctx->db, interned);

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
        _rd_db_segmentregs_find_vect(&ctx->db->segment_regs, interned);
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
        _rd_db_segmentregs_find_vect(&ctx->db->segment_regs, interned);
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
    return _rd_db_segmentregs_find_vect(&ctx->db->segment_regs, interned);
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
