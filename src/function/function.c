#include "function.h"
#include "core/context.h"
#include "function/chunk.h"
#include "io/flagsbuffer.h"

static bool _rd_function_is_valid_type(const RDContext* ctx, const char* type) {
    const RDTypeDef* tdef = rd_i_typedef_find(ctx, type);
    if(!tdef) return false;

    if(tdef->kind != RD_TKIND_FUNC) {
        RD_LOG_WARN("type '%s' is not a function", type);
        return false;
    }

    return true;
}

bool rd_i_function_declare(RDContext* ctx, const RDSegment* seg, usize idx,
                           const char* type) {
    if(rd_flagsbuffer_has_func(seg->flags, idx)) return false; // idempotent

    if(!_rd_function_is_valid_type(ctx, type)) type = NULL;

    RDAddress address = rd_i_abs(ctx, seg->rel_start + idx);

    rd_i_flagsbuffer_set_func(seg->flags, idx);
    rd_i_db_set_function(ctx, address, type);
    rd_emit_address_hook(ctx, "redasm.func_added", address);
    return true;
}

void rd_i_function_undeclare(RDContext* ctx, const RDSegment* seg, usize idx) {
    if(!rd_flagsbuffer_has_func(seg->flags, idx)) return; // idempotent

    RDAddress address = rd_i_abs(ctx, seg->rel_start + idx);

    rd_i_flagsbuffer_clear_func(seg->flags, idx);
    _rd_i_db_query_del_function(ctx, address);
    rd_i_chunks_del(ctx, address);
    rd_emit_address_hook(ctx, "redasm.func_removed", address);
}

void rd_i_function_set_type_def(RDContext* ctx, RDAddress address,
                                const RDTypeDef* tdef) {
    assert(tdef);
    assert(tdef->kind == RD_TKIND_FUNC);
    assert(rd_function_get_type_def(ctx, address)); // must be a function

    rd_i_db_set_function(ctx, address, tdef->name);
}

bool rd_i_function_find_rel(const RDContext* self, RDAddress address,
                            RDRelAddress* rel_entry) {
    const RDChunk* c = rd_i_chunks_find(self, address);
    if(!c) return false;

    if(rel_entry) *rel_entry = c->entry;
    return true;
}

bool rd_function_is_noret(const RDContext* self, RDAddress address) {
    return rd_i_function_is_noret(self, address,
                                  rd_function_get_type_def(self, address));
}

bool rd_function_find(const RDContext* self, RDAddress address,
                      RDAddress* entry) {
    RDRelAddress rel_entry;
    if(!rd_i_function_find_rel(self, address, &rel_entry)) return false;

    if(entry) *entry = rd_i_abs(self, rel_entry);
    return true;
}

const RDTypeDef* rd_function_get_type_def(const RDContext* self,
                                          RDAddress address) {
    const RDSegment* seg = rd_i_db_find_segment((RDContext*)self, address);
    if(!seg) return NULL;

    usize idx = rd_i_address2index(seg, address);
    if(!rd_flagsbuffer_has_func(seg->flags, idx)) return NULL;

    const char* type_name = NULL;
    if(!rd_i_db_get_function_type((RDContext*)self, address, &type_name))
        return NULL;

    const RDTypeDef* tdef =
        type_name ? rd_i_typedef_find(self, type_name) : NULL;
    if(tdef && tdef->kind != RD_TKIND_FUNC) tdef = NULL;

    return tdef ? tdef : rd_i_typedef_find(self, "function");
}

bool rd_i_function_is_noret(const RDContext* self, RDAddress address,
                            const RDTypeDef* tdef) {
    if(!tdef || tdef->kind != RD_TKIND_FUNC) return false;
    if(tdef->func_.is_noret) return true;

    const RDChunk* head = rd_i_chunks_find_head(self, address);
    return head && head->is_noret;
}
