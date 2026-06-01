#include "context.h"
#include "core/engine.h"
#include "core/mapping.h"
#include "core/segment.h"
#include "core/stringfinder.h"
#include "hooks.h"
#include "io/buffer.h"
#include "io/flagsbuffer.h"
#include "io/reader.h"
#include "support/containers.h"
#include "support/error.h"
#include "support/logging.h"
#include "support/utils.h"
#include "surface/items.h"
#include "surface/renderer.h"
#include "types/def.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>

#define RD_WORKER_QUEUE_SIZE 8192

static bool _rd_vect_contains_address(const RDAddressVect* v, RDAddress addr) {
    RDAddress* a;
    vect_each(a, v) {
        if(*a == addr) return true;
    }

    return false;
}

const RDAnalyzerPlugin* rd_analyzeritem_get_plugin(const RDAnalyzerItem* self) {
    return self->plugin;
}

bool rd_analyzeritem_is_selected(const RDAnalyzerItem* self) {
    return self->is_selected;
}

void rd_analyzeritem_select(RDAnalyzerItem* self, bool sel) {
    self->is_selected = sel;
}

RDContext* rd_i_context_create(const RDLoaderPlugin* lplugin, RDLoader* ldr,
                               const char* filepath, RDByteBuffer* input) {
    RDContext* self = rd_alloc0(1, sizeof(*self));

    self->loaderplugin = lplugin, self->loader = ldr;
    self->filepath = rd_strdup(filepath);
    self->input = input;
    self->input_reader = rd_i_reader_create((RDBuffer*)input);
    self->reader = rd_i_reader_create_flags(self);
    self->hooks = rd_i_hooks_create();
    self->min_string = RD_MIN_STRING_LENGTH;
    self->db = rd_i_db_create(self);
    self->kb = rd_i_kb_create();

    rd_i_registermap_init(&self->engine.current.registers);
    rd_i_strpool_init(&self->strings);
    rd_i_listing_init(&self->listing, NULL);
    rd_i_register_primitives(self);

    queue_reserve(&self->engine.qcall, RD_WORKER_QUEUE_SIZE);
    queue_reserve(&self->engine.qjump, RD_WORKER_QUEUE_SIZE);
    return self;
}

bool rd_i_get_name(RDContext* self, RDAddress address, bool autoname,
                   RDName* n) {
    const RDSegmentFull* seg = rd_i_db_find_segment(self, address);
    if(!seg) return false;

    if(rd_flagsbuffer_has_name(seg->flags, rd_i_address2index(seg, address)))
        return rd_i_db_get_name(self, address, n);

    if(!autoname) return false;
    n->confidence = RD_CONFIDENCE_AUTO;

    usize idx = rd_i_address2index(seg, address);
    const char* prefix = NULL;

    if(rd_i_flagsbuffer_has_type(seg->flags, idx)) {
        RDTypeFull t;
        bool ok = rd_i_db_get_type(self, address, &t);
        assert(ok && "type not found in database");

        if(!strcmp(t.base.name, "char") && t.base.count > 0)
            prefix = "str";
        else if(!strcmp(t.base.name, "char16") && t.base.count > 0)
            prefix = "str16";
        else
            prefix = t.base.name;
    }
    else if(rd_flagsbuffer_has_func(seg->flags, idx))
        prefix = "sub";
    else
        prefix = "loc";

    assert(prefix && "invalid name prefix");

    char* name = rd_i_format(&self->name_buf, "%s_%" PRIx64, prefix, address);
    n->value = rd_i_tolower(name);
    return true;
}

bool rd_i_set_name(RDContext* self, RDAddress address, const char* name,
                   RDConfidence c) {
    const RDSegmentFull* seg = rd_i_db_find_segment(self, address);
    if(!seg) return false;

    usize idx = rd_i_address2index(seg, address);
    if(rd_flagsbuffer_has_tail(seg->flags, idx)) {
        rd_i_add_problem(self, address, address,
                         "cannot set name '%s' on tail byte", name);
        return false;
    }

    bool hasname = rd_flagsbuffer_has_name(seg->flags, idx);
    RDName oldname = {0};

    if(hasname) {
        bool ok = rd_i_db_get_name(self, address, &oldname);
        assert(ok && "name not found in database");
    }

    if((!name || !(*name))) {
        if(!hasname || c < oldname.confidence) return false;
        rd_i_db_del_name(self, address);
        rd_i_flagsbuffer_undefine_name(seg->flags, idx);
        return true;
    }

    if(!hasname || c >= oldname.confidence) {
        // check if name exists at a different address
        RDAddress existing;
        const char* finalname = name;

        if(rd_i_db_get_address(self, name, &existing) && existing != address) {
            if(c == RD_CONFIDENCE_USER) {
                // user tried to set a duplicate name: reject with problem
                rd_i_add_problem(self, address, address,
                                 "duplicate name '%s' already exists at %llx",
                                 name, existing);
                return false;
            }
            if(c == RD_CONFIDENCE_LIBRARY) {
                RDName existingname = {0};
                if(rd_i_db_get_name(self, existing, &existingname) &&
                   existingname.confidence == RD_CONFIDENCE_USER) {
                    // USER already owns this name: reject
                    rd_i_add_problem(self, address, address,
                                     "duplicate name '%s' already owned by "
                                     "user at %llx",
                                     name, existing);
                    return false;
                }
            }

            // AUTO or LIBRARY vs non-USER: disambiguate with suffix
            int suffix = 1;
            RDAddress dummy;
            do {
                finalname =
                    rd_i_format(&self->name_buf, "%s_%d", name, suffix++);
            } while(rd_i_db_get_address(self, finalname, &dummy));
        }

        rd_i_db_set_name(self, address, finalname, c);
        rd_i_flagsbuffer_set_name(seg->flags, idx);

        return true;
    }

    return false;
}

const RDLoaderPlugin* rd_get_loader_plugin(const RDContext* self) {
    return self->loaderplugin;
}

RDProcessor* rd_get_processor(const RDContext* self) { return self->processor; }

const RDProcessorPlugin* rd_get_processor_plugin(const RDContext* self) {
    return self->processorplugin;
}

RDAnalyzerItemSlice rd_get_analyzer_plugins(const RDContext* self) {
    return vect_to_slice(RDAnalyzerItemSlice, &self->analyzerplugins);
}

RDLoader* rd_get_loader(const RDContext* self) { return self->loader; }

bool rd_map_segment(RDContext* self, const char* name, RDAddress addr,
                    RDAddress endaddr, u32 perm) {
    RDSegmentFull* s = rd_alloc0(1, sizeof(*s));
    s->base.name = rd_i_strpool_intern(&self->strings, name);
    s->base.start_address = addr;
    s->base.end_address = endaddr;
    s->base.perm = perm;
    s->flags = rd_i_flagsbuffer_create(endaddr - addr);

    if(rd_i_db_add_segment(self, s)) {
        LOG_INFO("mapping segment '%s' (Address: [%X, %X))", name, addr,
                 endaddr);
    }
    else
        rd_i_segment_destroy(s);

    return true;
}

bool rd_map_segment_n(RDContext* self, const char* name, RDAddress addr,
                      usize n, u32 perm) {
    return rd_map_segment(self, name, addr, addr + n, perm);
}

bool rd_map_input(RDContext* self, RDOffset off, RDAddress addr,
                  RDOffset endaddr) {
    RDInputMapping m = {
        .offset = off,
        .start_address = addr,
        .end_address = endaddr,
    };

    if(rd_i_db_add_mapping(self, m))
        LOG_INFO("mapping input at offset %X (Address: [%X, %X))", off, addr,
                 endaddr);

    return true;
}

bool rd_map_input_n(RDContext* self, RDOffset off, RDAddress addr, usize n) {
    return rd_map_input(self, off, addr, addr + n);
}

const RDSegment* rd_find_segment(const RDContext* self, RDAddress addr) {
    return (const RDSegment*)rd_i_db_find_segment(self, addr);
}

RDFunction* rd_i_find_function(const RDContext* self, RDAddress address) {
    const RDFunctionChunkVect* chunks = &self->functions.chunks;
    if(vect_is_empty(chunks)) return NULL;

    usize idx = vect_bsearch(chunks, &address, rd_i_functionchunk_kcmp_pred);
    if(idx == vect_length(chunks)) return NULL;

    const RDFunctionChunk* chunk = *vect_at(chunks, idx);

    idx = vect_bsearch(&self->functions, &chunk->func_address,
                       rd_i_function_kcmp_pred);
    if(idx == vect_length(&self->functions)) return NULL;

    return *vect_at(&self->functions, idx);
}

const RDFunction* rd_find_function(const RDContext* self, RDAddress address) {
    return rd_i_find_function(self, address);
}

bool rd_to_offset(const RDContext* self, RDAddress address, RDOffset* offset) {
    const RDMappingVect* mappings = rd_i_db_get_mappings(self);

    usize idx = vect_bsearch(mappings, &address, rd_i_mapping_kcmp_pred);
    if(idx == vect_length(mappings)) return NULL;

    RDInputMapping* m = vect_at(mappings, idx);
    if(offset) *offset = (address - m->start_address) + m->offset;
    return true;
}

bool rd_to_address(const RDContext* self, RDOffset offset, RDAddress* address) {
    const RDMappingVect* mappings = rd_i_db_get_mappings(self);

    RDInputMapping* m;
    vect_each(m, mappings) {
        usize size = m->end_address - m->start_address;
        if(offset >= m->offset && offset < m->offset + size) {
            if(address) *address = (offset - m->offset) + m->start_address;
            return true;
        }
    }

    return false;
}

bool rd_is_address(const RDContext* self, RDAddress address) {
    return rd_find_segment(self, address) != NULL;
}

bool rd_has_refs_from(const RDContext* self, RDAddress address) {
    const RDSegmentFull* seg = rd_i_db_find_segment(self, address);
    if(!seg) return false;

    usize idx = rd_i_address2index(seg, address);
    return rd_i_flagsbuffer_has_xref_out(seg->flags, idx);
}

bool rd_has_refs_to(const RDContext* self, RDAddress address) {
    const RDSegmentFull* seg = rd_i_db_find_segment(self, address);
    if(!seg) return false;

    usize idx = rd_i_address2index(seg, address);
    return rd_i_flagsbuffer_has_xref_in(seg->flags, idx);
}

bool rd_get_address(RDContext* self, const char* name, RDAddress* address) {
    if(!name) return false;
    if(rd_i_db_get_address(self, name, address)) return true;

    // check if it's an autogenerated name (format prefix_<hex>)
    const char* rhs = rd_i_strip_prefix(name);
    if(!rhs || rhs == name) return false;

    // validate right part
    for(const char* p = rhs; *p; p++) {
        if(!isxdigit((unsigned char)*p)) return false;
    }

    errno = 0;
    RDAddress val = strtoull(rhs, NULL, 16);
    panic_if(errno != 0, "rd_get_address: %s", strerror(errno));

    if(address) *address = val;
    return true;
}

const char* rd_get_comment(RDContext* self, RDAddress address) {
    const RDSegmentFull* seg = rd_i_db_find_segment(self, address);
    if(!seg) return NULL;

    if(rd_i_flagsbuffer_has_comment(seg->flags,
                                    rd_i_address2index(seg, address)))
        return rd_i_db_get_comment(self, address);

    return NULL;
}

const char* rd_get_name(RDContext* self, RDAddress address) {
    RDName n;
    if(rd_i_get_name(self, address, true, &n)) return n.value;
    return NULL;
}

const char* rd_to_dec(i64 v) { return rd_i_to_dec(v); }
const char* rd_to_hex(i64 v) { return rd_i_to_hex(v, 0); }

const char* rd_to_hexaddr(const RDContext* self, usize v) {
    const unsigned int PTR_SIZE = self->processorplugin->ptr_size;
    return rd_i_to_hex((isize)v, PTR_SIZE);
}

const char* rd_render_text(RDContext* self, RDAddress address) {
    LIndex idx = rd_i_listing_lower_bound(&self->listing, address);

    if(idx < vect_length(&self->listing)) {
        // find first code item
        while(idx < vect_length(&self->listing) &&
              vect_at(&self->listing, idx)->address == address) {

            const RDListingItem* item = vect_at(&self->listing, idx);
            if(item->kind == RD_LK_INSTRUCTION || item->kind == RD_LK_TYPE)
                break;

            idx++;
        }

        if(idx < vect_length(&self->listing) &&
           vect_at(&self->listing, idx)->address == address) {
            RDRenderer* r = rd_i_renderer_create(self, RD_RF_TEXT);
            rd_i_render_item_at(r, idx);
            rd_i_renderer_swap(r);
            rd_i_renderer_write_text(r, &self->str_buf);
            rd_i_renderer_destroy(r);
            return self->str_buf.data;
        }
    }

    vect_clear(&self->str_buf);
    vect_push(&self->str_buf, 0);
    return self->str_buf.data;
}

bool rd_auto_undefine(RDContext* self, RDAddress address) {
    return rd_i_undefine(self, address, RD_CONFIDENCE_AUTO);
}

bool rd_library_undefine(RDContext* self, RDAddress address) {
    return rd_i_undefine(self, address, RD_CONFIDENCE_LIBRARY);
}

bool rd_user_undefine(RDContext* self, RDAddress address) {
    return rd_i_undefine(self, address, RD_CONFIDENCE_USER);
}

bool rd_auto_undefine_n(RDContext* self, RDAddress address, usize n) {
    return rd_i_undefine_n(self, address, n, RD_CONFIDENCE_AUTO);
}

bool rd_library_undefine_n(RDContext* self, RDAddress address, usize n) {
    return rd_i_undefine_n(self, address, n, RD_CONFIDENCE_LIBRARY);
}

bool rd_user_undefine_n(RDContext* self, RDAddress address, usize n) {
    return rd_i_undefine_n(self, address, n, RD_CONFIDENCE_USER);
}

bool rd_auto_function(RDContext* self, RDAddress address, const char* name) {
    return rd_i_set_function(self, address, name, RD_CONFIDENCE_AUTO);
}

bool rd_library_function(RDContext* self, RDAddress address, const char* name) {
    return rd_i_set_function(self, address, name, RD_CONFIDENCE_LIBRARY);
}

bool rd_user_function(RDContext* self, RDAddress address, const char* name) {
    return rd_i_set_function(self, address, name, RD_CONFIDENCE_USER);
}

bool rd_auto_name(RDContext* self, RDAddress address, const char* name) {
    return rd_i_set_name(self, address, name, RD_CONFIDENCE_AUTO);
}

bool rd_library_name(RDContext* self, RDAddress address, const char* name) {
    return rd_i_set_name(self, address, name, RD_CONFIDENCE_LIBRARY);
}

bool rd_user_name(RDContext* self, RDAddress address, const char* name) {
    return rd_i_set_name(self, address, name, RD_CONFIDENCE_USER);
}

bool rd_set_noreturn(RDContext* self, RDAddress address) {
    const RDSegmentFull* seg = rd_i_db_find_segment(self, address);
    if(!seg) return false;

    usize idx = rd_i_address2index(seg, address);

    if(!rd_flagsbuffer_has_func(seg->flags, idx)) {
        rd_i_add_problem(self, address, address,
                         "cannot set noreturn on a non-function address");
        return false;
    }

    return rd_i_flagsbuffer_set_noret(seg->flags, idx);
}

bool rd_set_comment(RDContext* self, RDAddress address, const char* cmt) {
    const RDSegmentFull* seg = rd_i_db_find_segment(self, address);
    if(!seg) return false;

    usize idx = rd_i_address2index(seg, address);

    if(rd_flagsbuffer_has_tail(seg->flags, idx)) {
        rd_i_add_problem(self, address, address,
                         "cannot set comment on tail byte");
        return false;
    }

    if((!cmt || !(*cmt)) && rd_i_flagsbuffer_has_comment(seg->flags, idx)) {
        rd_i_flagsbuffer_undefine_comment(seg->flags, idx);
        rd_i_db_del_comment(self, address);
        return true;
    }

    if(cmt) {
        rd_i_flagsbuffer_set_comment(seg->flags, idx);
        rd_i_db_set_comment(self, address, cmt);
        return true;
    }

    return false;
}

bool rd_i_undefine(RDContext* self, RDAddress address, RDConfidence c) {
    return rd_i_undefine_n(self, address, 1, c);
}

bool rd_i_undefine_n(RDContext* self, RDAddress address, usize n,
                     RDConfidence c) {
    const RDSegmentFull* seg = rd_i_db_find_segment(self, address);
    if(!seg) return false;

    usize startidx = rd_i_address2index(seg, address), endidx = startidx + n;
    rd_i_flagsbuffer_expand_range(seg->flags, &startidx, &endidx);

    RDAddress startaddr = rd_i_index2address(seg, startidx);
    RDAddress endaddr = startaddr + (endidx - startidx);

    RDConfidence maxc =
        rd_i_db_get_undefine_confidence(self, startaddr, endaddr);
    if(maxc > c) return false;

    RDAddress curr = startaddr;

    for(usize i = startidx; i < endidx; i++, curr++) {
        if(rd_i_flagsbuffer_has_type(seg->flags, i)) {
            rd_i_db_del_type(self, curr);
            // FL_TYPE cleared by rd_i_flagsbuffer_undefine below
        }

        // remove references outgoing from this location
        if(rd_i_flagsbuffer_has_xref_out(seg->flags, i)) {
            const RDXRefVect* refs = rd_i_get_xrefs_from_ex(
                self, curr, RD_XR_NONE, &self->und_xrefs);

            const RDXRef* r;
            vect_each(r, refs) rd_i_del_xref(self, curr, r->address, c);
        }
    }

    rd_i_flagsbuffer_undefine(seg->flags, startidx, endidx - startidx);
    return true;
}

bool rd_i_set_function(RDContext* self, RDAddress address, const char* name,
                       RDConfidence c) {
    const RDSegmentFull* seg = rd_i_db_find_segment(self, address);
    if(!seg || !(seg->base.perm & RD_SP_X)) return false;

    usize index = rd_i_address2index(seg, address);

    // conflict checks
    if(rd_flagsbuffer_has_tail(seg->flags, index)) return false;
    if(rd_flagsbuffer_has_data(seg->flags, index)) return false;

    if(rd_i_engine_enqueue_call(self, address, name, c)) {
        rd_fire_address_hook(self, "redasm.function_found", address);
        return true;
    }

    return false;
}

void rd_destroy(RDContext* self) {
    if(!self) return;

    vect_destroy(&self->problem_buf);
    vect_destroy(&self->sym_buf);
    vect_destroy(&self->str_buf);
    vect_destroy(&self->imp_buf);
    vect_destroy(&self->name_buf);
    vect_destroy(&self->ovr_ops_buf);
    vect_destroy(&self->tdef_buf);
    vect_destroy(&self->type_buf);
    vect_destroy(&self->problems);
    vect_destroy(&self->xrefs_to);
    vect_destroy(&self->und_xrefs);
    vect_destroy(&self->xrefs_from);
    vect_destroy(&self->lift_buf);
    vect_destroy(&self->chunk_buf);
    vect_destroy(&self->imp_hint_buf);
    hmap_destroy(&self->engine.current.registers);
    vect_destroy(&self->exported);
    vect_destroy(&self->imported);
    rd_i_functionvect_destroy(&self->functions);
    rd_i_listing_deinit(&self->listing);
    rd_i_hooks_destroy(self->hooks);
    rd_i_engine_destroy(self);
    rd_i_kb_destroy(self->kb);
    rd_i_db_destroy(self->db);

    for(unsigned i = 0; i < rd_count_of(self->types); i++) {
        RDTypeDef** def;
        vect_each(def, &self->types[i]) rd_typedef_destroy(*def);
        vect_destroy(&self->types[i]);
    }

    RDAnalyzerItem** ai;
    vect_each(ai, &self->analyzerplugins) rd_free(*ai);
    vect_destroy(&self->analyzerplugins);

    if(self->processorplugin && self->processorplugin->destroy)
        self->processorplugin->destroy(self->processor);

    if(self->loaderplugin && self->loaderplugin->destroy)
        self->loaderplugin->destroy(self->loader);

    rd_i_reader_destroy(self->input_reader);
    rd_i_reader_destroy(self->reader);
    rd_i_buffer_destroy((RDBuffer*)self->input);
    rd_i_strpool_destroy(&self->strings);
    rd_free(self->filepath);
    rd_free(self);
}

RDReader* rd_get_reader(const RDContext* self) { return self->reader; }

RDReader* rd_get_input_reader(const RDContext* self) {
    return self->input_reader;
}

bool rd_read_u8(const RDContext* self, RDAddress address, u8* v) {
    const RDSegmentFull* s = rd_i_db_find_segment(self, address);
    if(!s) return 0;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_read_u8((const RDBuffer*)s->flags, idx, v);
}

bool rd_read_le16(const RDContext* self, RDAddress address, u16* v) {
    const RDSegmentFull* s = rd_i_db_find_segment(self, address);
    if(!s) return 0;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_read_le16((const RDBuffer*)s->flags, idx, v);
}

bool rd_read_le32(const RDContext* self, RDAddress address, u32* v) {
    const RDSegmentFull* s = rd_i_db_find_segment(self, address);
    if(!s) return 0;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_read_le32((const RDBuffer*)s->flags, idx, v);
}

bool rd_read_le64(const RDContext* self, RDAddress address, u64* v) {
    const RDSegmentFull* s = rd_i_db_find_segment(self, address);
    if(!s) return 0;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_read_le64((const RDBuffer*)s->flags, idx, v);
}

bool rd_read_be16(const RDContext* self, RDAddress address, u16* v) {
    const RDSegmentFull* s = rd_i_db_find_segment(self, address);
    if(!s) return 0;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_read_be16((const RDBuffer*)s->flags, idx, v);
}

bool rd_read_be32(const RDContext* self, RDAddress address, u32* v) {
    const RDSegmentFull* s = rd_i_db_find_segment(self, address);
    if(!s) return false;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_read_be32((const RDBuffer*)s->flags, idx, v);
}

bool rd_read_be64(const RDContext* self, RDAddress address, u64* v) {
    const RDSegmentFull* s = rd_i_db_find_segment(self, address);
    if(!s) return false;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_read_be64((const RDBuffer*)s->flags, idx, v);
}

bool rd_read_ptr(const RDContext* ctx, RDAddress address, RDAddress* v) {
    bool is_be = ctx->processorplugin->flags & RD_PF_BE;

    switch(ctx->processorplugin->ptr_size) {
        case sizeof(u8): {
            u8 ptr_v;
            if(!rd_read_u8(ctx, address, &ptr_v)) return false;
            *v = (RDAddress)ptr_v;
            return true;
        }

        case sizeof(u16): {
            u16 ptr_v;
            if(is_be ? !rd_read_be16(ctx, address, &ptr_v)
                     : !rd_read_le16(ctx, address, &ptr_v))
                return false;

            *v = (RDAddress)ptr_v;
            return true;
        }

        case sizeof(u32): {
            u32 ptr_v;
            if(is_be ? !rd_read_be32(ctx, address, &ptr_v)
                     : !rd_read_le32(ctx, address, &ptr_v))
                return false;

            *v = (RDAddress)ptr_v;
            return true;
        }

        case sizeof(u64): {
            u64 ptr_v;
            if(is_be ? !rd_read_be64(ctx, address, &ptr_v)
                     : !rd_read_le64(ctx, address, &ptr_v))
                return false;

            *v = (RDAddress)ptr_v;
            return true;
        }

        default: break;
    }

    return false;
}

bool rd_follow_ptr(RDContext* ctx, RDAddress address, RDAddress* v) {
    RDAddressVect visited = {0};
    RDAddress current = address;

    while(true) {
        if(_rd_vect_contains_address(&visited, current)) break; // loop detected
        vect_push(&visited, current);

        RDTypeFull t;
        if(!rd_i_get_type(ctx, current, &t) || !rd_type_is_ptr(&t.base)) break;

        RDAddress next;
        if(!rd_read_ptr(ctx, current, &next) || !rd_is_address(ctx, next))
            break;

        current = next;
    }

    vect_destroy(&visited);

    if(current == address) return false; // didn't move
    if(v) *v = current;
    return true;
}

bool rd_expect_u8(const RDContext* self, RDAddress address, u8 v) {
    const RDSegmentFull* s = rd_i_db_find_segment(self, address);
    if(!s) return false;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_expect_u8((const RDBuffer*)s->flags, idx, v);
}

bool rd_expect_le16(const RDContext* self, RDAddress address, u16 v) {
    const RDSegmentFull* s = rd_i_db_find_segment(self, address);
    if(!s) return false;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_expect_le16((const RDBuffer*)s->flags, idx, v);
}

bool rd_expect_le32(const RDContext* self, RDAddress address, u32 v) {
    const RDSegmentFull* s = rd_i_db_find_segment(self, address);
    if(!s) return false;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_expect_le32((const RDBuffer*)s->flags, idx, v);
}

bool rd_expect_le64(const RDContext* self, RDAddress address, u64 v) {
    const RDSegmentFull* s = rd_i_db_find_segment(self, address);
    if(!s) return false;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_expect_le64((const RDBuffer*)s->flags, idx, v);
}

bool rd_expect_be16(const RDContext* self, RDAddress address, u16 v) {
    const RDSegmentFull* s = rd_i_db_find_segment(self, address);
    if(!s) return false;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_expect_be16((const RDBuffer*)s->flags, idx, v);
}

bool rd_expect_be32(const RDContext* self, RDAddress address, u32 v) {
    const RDSegmentFull* s = rd_i_db_find_segment(self, address);
    if(!s) return false;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_expect_be32((const RDBuffer*)s->flags, idx, v);
}

bool rd_expect_be64(const RDContext* self, RDAddress address, u64 v) {
    const RDSegmentFull* s = rd_i_db_find_segment(self, address);
    if(!s) return false;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_expect_be64((const RDBuffer*)s->flags, idx, v);
}

usize rd_read(const RDContext* self, RDAddress address, void* data, usize n) {
    const RDSegmentFull* s = rd_i_db_find_segment(self, address);
    if(!s) return 0;
    return rd_i_buffer_read((RDBuffer*)s->flags, rd_i_address2index(s, address),
                            data, n);
}

bool rd_write_u8(RDContext* self, RDAddress address, u8 v) {
    const RDSegmentFull* s = rd_i_db_find_segment(self, address);
    if(!s) return false;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_write_u8((RDBuffer*)s->flags, idx, v);
}

bool rd_write_le16(RDContext* self, RDAddress address, u16 v) {
    const RDSegmentFull* s = rd_i_db_find_segment(self, address);
    if(!s) return false;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_write_le16((RDBuffer*)s->flags, idx, v);
}

bool rd_write_le32(RDContext* self, RDAddress address, u32 v) {
    const RDSegmentFull* s = rd_i_db_find_segment(self, address);
    if(!s) return false;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_write_le32((RDBuffer*)s->flags, idx, v);
}

bool rd_write_le64(RDContext* self, RDAddress address, u64 v) {
    const RDSegmentFull* s = rd_i_db_find_segment(self, address);
    if(!s) return false;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_write_le64((RDBuffer*)s->flags, idx, v);
}

bool rd_write_be16(RDContext* self, RDAddress address, u16 v) {
    const RDSegmentFull* s = rd_i_db_find_segment(self, address);
    if(!s) return false;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_write_be16((RDBuffer*)s->flags, idx, v);
}

bool rd_write_be32(RDContext* self, RDAddress address, u32 v) {
    const RDSegmentFull* s = rd_i_db_find_segment(self, address);
    if(!s) return false;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_write_be32((RDBuffer*)s->flags, idx, v);
}

bool rd_write_be64(RDContext* self, RDAddress address, u64 v) {
    const RDSegmentFull* s = rd_i_db_find_segment(self, address);
    if(!s) return false;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_write_be64((RDBuffer*)s->flags, idx, v);
}

usize rd_write(RDContext* self, RDAddress address, const void* data, usize n) {
    const RDSegmentFull* s = rd_i_db_find_segment(self, address);
    if(!s) return 0;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_write((RDBuffer*)s->flags, idx, data, n);
}

bool rd_fill(RDContext* self, RDAddress address, usize n) {
    const RDSegmentFull* s = rd_i_db_find_segment(self, address);
    if(!s) return false;

    usize idx = rd_i_address2index(s, address);
    usize end_idx = rd_i_min(idx + n, rd_flagsbuffer_get_length(s->flags));

    for(usize i = idx; i < end_idx; i++)
        rd_i_flagsbuffer_set_value(s->flags, i, 0);

    return true;
}

const char* rd_get_imported_hint(RDContext* ctx, const char* name) {
    return rd_i_get_imported_hint(ctx, name, &ctx->imp_hint_buf);
}

const char* rd_get_imported_ord_hint(RDContext* ctx, const char* module,
                                     u32 ordinal) {
    return rd_i_get_imported_ord_hint(ctx, module, ordinal, &ctx->imp_hint_buf);
}

RDProblemSlice rd_get_all_problems(const RDContext* self) {
    return vect_to_slice(RDProblemSlice, &self->problems);
}

RDSegmentSlice rd_get_all_segments(const RDContext* self) {
    const RDSegmentVect* segments = rd_i_db_get_segments(self);

    return (RDSegmentSlice){
        .data = (const RDSegment**)segments->data,
        .length = segments->length,
    };
}

RDInputMappingSlice rd_get_all_mappings(const RDContext* self) {
    return vect_to_slice(RDInputMappingSlice, rd_i_db_get_mappings(self));
}

RDFunctionSlice rd_get_all_functions(const RDContext* self) {
    return (RDFunctionSlice){
        .data = (const RDFunction**)self->functions.data,
        .length = self->functions.length,
    };
}

RDAddressSlice rd_get_all_exported(const RDContext* self) {
    return vect_to_slice(RDAddressSlice, &self->exported);
}

RDAddressSlice rd_get_all_imported(const RDContext* self) {
    return vect_to_slice(RDAddressSlice, &self->imported);
}

RDSymbolSlice rd_get_all_symbols(const RDContext* self) {
    return vect_to_slice(RDSymbolSlice, &self->listing.symbols);
}

const RDXRefVect* rd_i_get_xrefs_from_ex(RDContext* self, RDAddress fromaddr,
                                         RDXRefType type, RDXRefVect* r) {
    vect_clear(r);

    const RDSegmentFull* seg = rd_i_db_find_segment(self, fromaddr);
    if(!seg) return r;

    usize idx = rd_i_address2index(seg, fromaddr);
    if(!rd_i_flagsbuffer_has_xref_out(seg->flags, idx)) return r;

    rd_i_db_get_xrefs_from(self, fromaddr, type, r);
    return r;
}

const RDXRefVect* rd_i_get_xrefs_to_ex(RDContext* self, RDAddress toaddr,
                                       RDXRefType type, RDXRefVect* r) {
    vect_clear(r);

    const RDSegmentFull* seg = rd_i_db_find_segment(self, toaddr);
    if(!seg) return r;

    usize idx = rd_i_address2index(seg, toaddr);
    if(!rd_i_flagsbuffer_has_xref_in(seg->flags, idx)) return r;

    rd_i_db_get_xrefs_to(self, toaddr, type, r);
    return r;
}

const RDXRefVect* rd_i_get_xrefs_from(RDContext* self, RDAddress fromaddr,
                                      RDXRefType type) {
    return rd_i_get_xrefs_from_ex(self, fromaddr, type, &self->xrefs_from);
}

const RDXRefVect* rd_i_get_xrefs_to(RDContext* self, RDAddress toaddr,
                                    RDXRefType type) {
    return rd_i_get_xrefs_to_ex(self, toaddr, type, &self->xrefs_to);
}

const char* rd_symbol_to_string(const RDSymbol* self, RDContext* ctx) {
    switch(self->kind) {
        case RD_SYMBOL_SEGMENT: {
            const RDSegmentFull* seg = rd_i_db_find_segment(ctx, self->address);
            return seg ? seg->base.name : NULL;
        }

        case RD_SYMBOL_STRING: {
            const RDSegmentFull* seg = rd_i_db_find_segment(ctx, self->address);
            assert(seg &&
                   "cannot convert symbol to string, type outside of segments");

            RDTypeFull t;
            bool ok = rd_i_get_type(ctx, self->address, &t);
            assert(ok && "cannot convert symbol to string, type not found");

            usize char_sz = rd_i_size_of(ctx, t.base.name, 0, RD_TYPE_NONE);
            usize n = char_sz * t.base.count;
            usize idx = rd_i_address2index(seg, self->address);
            // reserve at least these bytes, +2 for quoting, +1 null terminator
            vect_reserve(&ctx->sym_buf, t.base.count + 3);
            vect_clear(&ctx->sym_buf);
            vect_push(&ctx->sym_buf, '\"');

            for(usize i = 0; i < n; i += char_sz) {
                u8 v;
                ok = rd_flagsbuffer_get_value(seg->flags, idx + i, &v);
                assert(ok &&
                       "cannot convert symbol to string, missing character");
                if(!v) break; // null terminator

                const char* s = rd_i_escape_char((char)v, true);

                while(*s)
                    vect_push(&ctx->sym_buf, *s++);
            }

            vect_push(&ctx->sym_buf, '\"');
            vect_push(&ctx->sym_buf, 0);
            return ctx->sym_buf.data;
        }

        default: break;
    }

    return rd_get_name(ctx, self->address);
}

bool rd_i_set_imported(RDContext* self, RDAddress address, const char* name,
                       const RDImported* imp) {
    const RDSegmentFull* seg = rd_i_db_find_segment(self, address);
    if(!seg) return false;

    usize idx = rd_i_address2index(seg, address);

    if(rd_flagsbuffer_has_tail(seg->flags, idx)) {
        rd_i_add_problem(self, address, address,
                         "cannot set imported on tail byte");
        return false;
    }

    if(imp->ordinal.has_value) {
        name = rd_i_get_imported_ord_hint(self, imp->module, imp->ordinal.value,
                                          &self->imp_buf);
    }
    else if(name)
        name = rd_i_get_imported_hint(self, name, &self->imp_buf);
    else {
        rd_i_add_problem(self, address, address, "invalid import name");
        return false;
    }

    if(name) {
        rd_i_set_name(self, address, name, RD_CONFIDENCE_LIBRARY);

        if(rd_i_kb_is_noret(self, rd_i_strip_prefix(name)))
            rd_i_set_noret(self, address);
    }

    const unsigned int PTR_SIZE = self->processorplugin->ptr_size;
    const char* ptrtype = rd_integral_from_size(PTR_SIZE);
    rd_i_set_type(self, address, ptrtype, 0, RD_TYPE_PTR, RD_CONFIDENCE_AUTO);

    if(!rd_i_flagsbuffer_set_imported(seg->flags, idx)) return false;

    rd_i_db_set_imported(self, address, imp);

    usize imp_idx =
        vect_lower_bound(&self->imported, &address, rd_i_address_kcmp_pred);

    if(imp_idx == vect_length(&self->imported) ||
       *vect_at(&self->imported, imp_idx) != address)
        vect_ins(&self->imported, imp_idx, address);

    return true;
}

bool rd_i_add_xref(RDContext* self, RDAddress fromaddr, RDAddress toaddr,
                   RDXRefType type, RDConfidence c) {
    const RDSegmentFull* fromseg = rd_i_db_find_segment(self, fromaddr);
    const RDSegmentFull* toseg = rd_i_db_find_segment(self, toaddr);
    if(!fromseg || !toseg) return false;

    usize fromidx = rd_i_address2index(fromseg, fromaddr);
    usize toidx = rd_i_address2index(toseg, toaddr);

    if(rd_flagsbuffer_has_tail(fromseg->flags, fromidx)) {
        rd_i_add_problem(self, fromaddr, fromaddr, "ref FROM tail byte");
        return false;
    }

    bool isreftotail = rd_flagsbuffer_has_tail(toseg->flags, toidx);

    if(isreftotail) // report only
        rd_i_add_problem(self, fromaddr, toaddr, "ref TO tail byte");

    RDXRefFull xref;
    if(rd_i_db_get_xref(self, fromaddr, toaddr, &xref) && xref.confidence > c)
        return false;

    switch(type) {
        case RD_DR_READ:
        case RD_DR_WRITE:
        case RD_DR_ADDRESS: {
            rd_i_db_add_xref(self, fromaddr, toaddr, type, c);
            rd_i_flagsbuffer_set_xref_out(fromseg->flags, fromidx);

            if(!isreftotail) rd_i_flagsbuffer_set_xref_in(toseg->flags, toidx);
            break;
        }

        case RD_CR_JUMP: {
            rd_i_db_add_xref(self, fromaddr, toaddr, type, c);
            rd_i_flagsbuffer_set_xref_out(fromseg->flags, fromidx);

            if(!isreftotail) {
                rd_i_flagsbuffer_set_xref_in(toseg->flags, toidx);
                rd_i_engine_enqueue_jump(self, toaddr);
            }

            break;
        }

        case RD_CR_CALL: {
            rd_i_db_add_xref(self, fromaddr, toaddr, type, c);
            rd_i_flagsbuffer_set_xref_out(fromseg->flags, fromidx);

            if(!isreftotail) {
                rd_i_flagsbuffer_set_xref_in(toseg->flags, toidx);
                rd_i_engine_enqueue_call(self, toaddr, NULL, c);
            }

            break;
        }

        default: return false;
    }

    rd_fire_xref_hook(self, "redasm.xref_added", fromaddr, toaddr, type);
    return true;
}

bool rd_i_del_xref(RDContext* self, RDAddress fromaddr, RDAddress toaddr,
                   RDConfidence c) {
    const RDSegmentFull* fromseg = rd_i_db_find_segment(self, fromaddr);
    const RDSegmentFull* toseg = rd_i_db_find_segment(self, toaddr);
    if(!fromseg || !toseg) return false;

    usize fromidx = rd_i_address2index(fromseg, fromaddr);
    usize toidx = rd_i_address2index(toseg, toaddr);

    if(!rd_i_flagsbuffer_has_xref_out(fromseg->flags, fromidx)) return false;
    if(!rd_i_flagsbuffer_has_xref_in(toseg->flags, toidx)) return false;
    if(!rd_i_db_del_xref(self, fromaddr, toaddr, c)) return false;

    if(!rd_i_db_has_xrefs_from(self, fromaddr))
        rd_i_flagsbuffer_undefine_xref_out(fromseg->flags, fromidx);

    if(!rd_i_db_has_xrefs_to(self, toaddr))
        rd_i_flagsbuffer_undefine_xref_in(toseg->flags, toidx);

    return true;
}

bool rd_get_entry_point(const RDContext* self, RDAddress* address) {
    if(self->entry_point.has_value) {
        if(address) *address = self->entry_point.value;
        return true;
    }

    // try to fallback to the first address available
    const RDSegmentVect* segments = rd_i_db_get_segments(self);

    if(!vect_is_empty(segments)) {
        if(address) *address = (*vect_first(segments))->base.start_address;

        return true;
    }

    return false;
}

bool rd_set_entry_point(RDContext* self, RDAddress address, const char* name) {
    if(self->entry_point.has_value && self->entry_point.value != address) {
        rd_i_add_problem(self, self->entry_point.value, address,
                         "entry point redefined");
    }

    const char* n = name;

    if(!n || !(*n)) {
        const RDLoaderPlugin* ldr = self->loaderplugin;
        assert(ldr && "invalid loader plugin");
        n = rd_i_format(&self->name_buf, "%s_entry_point_%llx", ldr->id,
                        address);
    }

    assert(n && "invalid entry point name");

    if(rd_library_function(self, address, n) &&
       rd_set_exported(self, address, NULL)) {
        rd_i_db_set_entry_point(self, address);
        optional_set(&self->entry_point, address);
        return true;
    }

    return false;
}

bool rd_set_exported(RDContext* self, RDAddress address, const char* name) {
    const RDSegmentFull* seg = rd_i_db_find_segment(self, address);

    if(!seg) {
        rd_i_add_problem(self, address, address,
                         "skipping export '%s' to unmapped address",
                         name ? name : "<unnamed>");

        return false;
    }

    usize idx = rd_i_address2index(seg, address);

    if(rd_flagsbuffer_has_tail(seg->flags, idx)) {
        rd_i_add_problem(self, address, address,
                         "cannot set exported on tail byte");
        return false;
    }

    if(name) rd_i_set_name(self, address, name, RD_CONFIDENCE_LIBRARY);
    if(!rd_i_flagsbuffer_set_exported(seg->flags, idx)) return false;

    usize exp_idx =
        vect_lower_bound(&self->exported, &address, rd_i_address_kcmp_pred);

    if(exp_idx == vect_length(&self->exported) ||
       *vect_at(&self->exported, exp_idx) != address)
        vect_ins(&self->exported, exp_idx, address);

    return true;
}

bool rd_set_imported(RDContext* self, RDAddress address, const char* module,
                     const char* name) {
    return rd_i_set_imported(self, address, name,
                             &(RDImported){
                                 .module = module,
                                 .ordinal = {.has_value = false},
                             });
}

bool rd_set_imported_ord(RDContext* self, RDAddress address, const char* module,
                         u32 ord) {
    return rd_i_set_imported(self, address, NULL,
                             &(RDImported){
                                 .module = module,
                                 .ordinal = {.value = ord, .has_value = true},
                             });
}

bool rd_get_imported(RDContext* self, RDAddress address, RDImported* imp) {
    const RDSegmentFull* seg = rd_i_db_find_segment(self, address);
    if(!seg) return false;

    usize idx = rd_i_address2index(seg, address);
    if(!rd_flagsbuffer_has_imported(seg->flags, idx)) return false;
    return rd_i_db_get_imported(self, address, imp);
}

bool rd_i_set_noret(RDContext* self, RDAddress address) {
    const RDSegmentFull* seg = rd_i_db_find_segment(self, address);
    if(!seg) return false;

    usize idx = rd_i_address2index(seg, address);
    if(rd_flagsbuffer_has_tail(seg->flags, idx)) return false;
    if(rd_flagsbuffer_has_noret(seg->flags, idx)) return false;

    rd_i_flagsbuffer_set_noret(seg->flags, idx);

    usize len = rd_i_flagsbuffer_get_range_length(seg->flags, idx);
    assert(len > 0);

    idx += len; // un-FLOW next instruction

    if(rd_flagsbuffer_has_flow(seg->flags, idx))
        rd_i_flagsbuffer_undefine_flow(seg->flags, idx);

    return true;
}

const char* rd_i_get_imported_hint(RDContext* ctx, const char* name,
                                   RDCharVect* v) {
    RD_UNUSED(ctx);

    if(name) return rd_i_format(v, "__imp_%s", name);
    return NULL;
}

const char* rd_i_get_imported_ord_hint(RDContext* ctx, const char* module,
                                       u32 ordinal, RDCharVect* v) {
    const char* name = rd_i_kb_find_ordinal_name(ctx, module, ordinal);
    if(name) return rd_i_get_imported_hint(ctx, name, v);

    if(module) return rd_i_format(v, "__imp_%s_%" PRIu32, module, ordinal);
    return rd_i_format(v, "__imp_%" PRIu32, ordinal);
}

void rd_i_add_problem(RDContext* self, RDAddress from, RDAddress address,
                      const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    const char* result = rd_i_vformat(&self->problem_buf, fmt, args);
    va_end(args);

    RDProblem p = {
        .from_address = from,
        .address = address,
        .message = rd_i_strpool_intern(&self->strings, result),
    };

    vect_push(&self->problems, p);
    rd_i_db_add_problem(self, &p);
}

bool rd_add_xref(RDContext* self, RDAddress fromaddr, RDAddress toaddr,
                 RDXRefType type) {
    return rd_i_add_xref(self, fromaddr, toaddr, type, RD_CONFIDENCE_AUTO);
}

RDXRefSlice rd_get_xrefs_from(RDContext* self, RDAddress fromaddr,
                              RDXRefType type) {
    const RDXRefVect* refs = rd_i_get_xrefs_from(self, fromaddr, type);
    return vect_to_slice(RDXRefSlice, refs);
}

RDXRefSlice rd_get_xrefs_to(RDContext* self, RDAddress toaddr,
                            RDXRefType type) {
    const RDXRefVect* refs = rd_i_get_xrefs_to(self, toaddr, type);
    return vect_to_slice(RDXRefSlice, refs);
}

RDLoadAddressing rd_get_load_addressing(const RDContext* self) {
    return self->addressing;
}

unsigned int rd_get_min_string(const RDContext* self) {
    return self->min_string;
}

void rd_set_min_string(RDContext* self, unsigned int l) {
    self->min_string = l;
}

bool rd_operand_as_address(RDContext* self, RDAddress address, int index) {
    if(index >= RD_MAX_OPERANDS) return false;

    const RDSegmentFull* seg = rd_i_db_find_segment(self, address);
    if(!seg) return false;

    usize idx = rd_i_address2index(seg, address);

    if(!rd_flagsbuffer_has_code(seg->flags, idx) ||
       rd_flagsbuffer_has_tail(seg->flags, idx))
        return false;

    RDInstruction instr;
    if(!rd_decode(self, address, &instr)) return false;

    const RDOperand* op = &instr.operands[index];
    if(op->kind != RD_OP_IMM || !rd_is_address(self, op->imm)) return false;

    rd_i_flagsbuffer_set_op_over(seg->flags, idx);
    rd_i_db_set_ovr_operand(self, address, index);
    rd_i_add_xref(self, address, op->imm, RD_DR_ADDRESS, RD_CONFIDENCE_USER);
    return true;
}

bool rd_operand_as_immediate(RDContext* self, RDAddress address, int index) {
    if(index >= RD_MAX_OPERANDS) return false;

    const RDSegmentFull* seg = rd_i_db_find_segment(self, address);
    if(!seg) return false;

    usize flag_idx = rd_i_address2index(seg, address);

    if(!rd_flagsbuffer_has_code(seg->flags, flag_idx) ||
       rd_flagsbuffer_has_tail(seg->flags, flag_idx) ||
       !rd_i_flagsbuffer_has_op_over(seg->flags, flag_idx))
        return false;

    RDInstruction instr;
    if(!rd_decode(self, address, &instr)) return false;

    const RDOperand* op = &instr.operands[index];
    if(op->kind != RD_OP_ADDR) return false;

    rd_i_del_xref(self, address, op->addr, RD_CONFIDENCE_USER);
    rd_i_db_del_ovr_operand(self, address, index);

    if(!rd_i_db_has_ovr_operand(self, address))
        rd_i_flagsbuffer_undefine_op_over(seg->flags, flag_idx);

    return true;
}
