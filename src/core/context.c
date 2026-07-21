#include "context.h"
#include "core/engine.h"
#include "core/mapping.h"
#include "core/segment.h"
#include "hooks.h"
#include "io/buffer.h"
#include "io/flagsbuffer.h"
#include "io/reader.h"
#include "plugins/analyzer.h"
#include "support/containers.h"
#include "support/error.h"
#include "support/scratch.h"
#include "support/utils.h"
#include "surface/items.h"
#include "surface/renderer.h"
#include "types/def.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <redasm/support/logging.h>

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

RDContext* rd_i_context_create(const RDLoaderPlugin* lplugin,
                               const RDProcessorPlugin* pplugin,
                               RDByteBuffer* input, const char* workingdir,
                               const char* filename, const char* dbpath) {
    assert(pplugin);
    RDContext* self = rd_alloc0(1, sizeof(*self));

    self->loaderplugin = lplugin;
    self->processorplugin = pplugin;
    self->working_dir = rd_strdup(workingdir);
    self->file_name = rd_strdup(filename);
    self->input = input;
    self->input_reader = rd_i_reader_create((RDBuffer*)input);
    self->reader = rd_i_reader_create_flags(self);
    self->hooks = rd_i_hooks_create();
    self->db = rd_i_db_create(dbpath);
    self->kb = rd_i_kb_create();

    assert(self->working_dir);
    assert(self->file_name);

    rd_i_registermap_init(&self->engine.current.registers);
    rd_i_strpool_init(&self->strings);
    rd_i_register_primitives(self);

    queue_reserve(&self->engine.qcall, RD_WORKER_QUEUE_SIZE);
    queue_reserve(&self->engine.qjump, RD_WORKER_QUEUE_SIZE);

    if(self->processorplugin->create)
        self->processor = self->processorplugin->create(self->processorplugin);

    return self;
}

bool rd_i_get_name(RDContext* self, RDAddress address, bool autoname,
                   RDName* n) {
    const RDSegmentFull* seg = rd_i_db_find_segment(self, address);
    if(!seg) return false;

    if(rd_flagsbuffer_has_name(seg->flags, rd_i_address2index(seg, address)))
        return rd_i_db_get_name(self, address, n);

    if(!autoname) return false;
    n->confidence = RD_CONFIDENCE_PLACEHOLDER;

    usize idx = rd_i_address2index(seg, address);
    const char* s = NULL;

    if(rd_flagsbuffer_has_exported(seg->flags, idx) ||
       rd_flagsbuffer_has_imported(seg->flags, idx)) {
        RDExternal ext;

        if(rd_i_db_get_external(self, address, &ext)) {
            const char* kind_prefix = NULL;

            if(ext.kind == RD_EXT_IMPORTED)
                kind_prefix = "imp";
            else if(ext.kind == RD_EXT_EXPORTED)
                kind_prefix = "exp";
            else
                unreachable();

            if(ext.ordinal.has_value) {
                if(ext.module) {
                    s = rd_i_format(&self->name_buf, "%s_%s_#%" PRId32,
                                    kind_prefix, ext.module, ext.ordinal.value);
                }
                else {
                    s = rd_i_format(&self->name_buf, "%s_#%" PRId32,
                                    kind_prefix, ext.ordinal.value);
                }
            }
            else if(ext.module) {
                s = rd_i_format(&self->name_buf, "%s_%s_%" PRIx64, kind_prefix,
                                ext.module, address);
            }
            else {
                s = rd_i_format(&self->name_buf, "%s_%" PRIx64, kind_prefix,
                                address);
            }
        }
    }

    if(!s) {
        if(rd_flagsbuffer_has_type(seg->flags, idx)) {
            RDTypeFull t;
            bool ok = rd_i_db_get_type(self, address, &t);
            assert(ok && "type not found in database");

            if(!strcmp(t.base.def->name, "char") && t.base.count > 0)
                s = "str";
            else if(!strcmp(t.base.def->name, "char16") && t.base.count > 0)
                s = "str16";
            else
                s = t.base.def->name;
        }
        else if(rd_flagsbuffer_has_func(seg->flags, idx))
            s = "sub";
        else
            s = "loc";

        assert(s && "invalid name prefix");
        rd_i_format(&self->name_buf, "%s_%" PRIx64, s, address);
    }

    n->value = rd_i_tolower(self->name_buf.data);
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
            RDAddress curraddr;
            do {
                finalname =
                    rd_i_format(&self->name_buf, "%s_%d", name, suffix++);
            } while(rd_i_db_get_address(self, finalname, &curraddr) &&
                    curraddr != address);

            // try dedup in finalization
            vect_push(
                &self->pending_renames,
                (RDPendingRename){
                    .address = address,
                    .name =
                        {
                            .value = rd_i_strpool_intern(&self->strings, name),
                            .confidence = c,
                        },
                });
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

const RDProcessorPlugin* rd_get_processor_plugin(const RDContext* self) {
    return self->processorplugin;
}

RDAnalyzerItemSlice rd_get_analyzer_plugins(const RDContext* self) {
    return vect_to_slice(RDAnalyzerItemSlice, &self->analyzerplugins);
}

bool rd_map_segment(RDContext* self, const char* name, RDAddress addr,
                    RDAddress endaddr, u32 perm) {
    if(!name) { // autogenerate name
        name = rd_i_format(&self->seg_buf, "seg%03d",
                           (int)vect_length(&self->db->segments));
    }

    // FIXME: unit argument is missing
    RDSegmentFull* s = rd_i_segment_create(self, name, addr, endaddr, perm, 0);
    if(!s) return false;

    if(rd_i_db_add_segment(self, s)) {
        RD_LOG_INFO("mapping segment '%s' (Address: [%X, %X))", name, addr,
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
        RD_LOG_INFO("mapping input at offset %X (Address: [%X, %X))", off, addr,
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
    return (RDFunction*)chunk->func;
}

const RDFunction* rd_find_function(const RDContext* self, RDAddress address) {
    return rd_i_find_function(self, address);
}

bool rd_to_offset(const RDContext* self, RDAddress address, RDOffset* offset) {
    const RDMappingVect* mappings = rd_i_db_get_mappings(self);

    usize idx = vect_bsearch(mappings, &address, rd_i_mapping_kcmp_pred);
    if(idx == vect_length(mappings)) return false;

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
    vect_clear(&self->str_buf);

    RDRenderer* r = NULL;
    const RDSegmentFull* seg = rd_i_db_find_segment(self, address);
    if(!seg) goto done;

    r = rd_i_renderer_create(self, RD_RF_TEXT);
    rd_i_render_item(r, seg, rd_i_address2index(seg, address), 0);
    rd_i_renderer_swap(r);
    rd_i_renderer_write_text(r, &self->str_buf);
    rd_i_renderer_destroy(r);

done:
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

bool rd_placeholder_function(RDContext* self, RDAddress address,
                             const char* name) {
    return rd_i_set_function(self, address, name, RD_CONFIDENCE_PLACEHOLDER);
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

bool rd_placeholder_name(RDContext* self, RDAddress address, const char* name) {
    return rd_i_set_name(self, address, name, RD_CONFIDENCE_PLACEHOLDER);
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
        if(rd_flagsbuffer_has_type(seg->flags, i)) {
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
    vect_destroy(&self->seg_buf);
    vect_destroy(&self->imp_buf);
    vect_destroy(&self->autoname_buf);
    vect_destroy(&self->name_buf);
    vect_destroy(&self->ovr_ops_buf);
    vect_destroy(&self->tdef_buf);
    vect_destroy(&self->type_buf);
    vect_destroy(&self->addr_type_buf);
    vect_destroy(&self->symbols);
    vect_destroy(&self->problems);
    vect_destroy(&self->pending_renames);
    vect_destroy(&self->xrefs_to);
    vect_destroy(&self->und_xrefs);
    vect_destroy(&self->xrefs_from);
    vect_destroy(&self->lift_buf);
    vect_destroy(&self->chunk_buf);
    vect_destroy(&self->externals);
    hmap_destroy(&self->engine.current.registers);
    rd_i_analyzeritemvect_destroy(&self->analyzerplugins);
    rd_i_functionvect_destroy(&self->functions);
    rd_i_hooks_destroy(self->hooks);
    rd_i_engine_destroy(self);
    rd_i_kb_destroy(self->kb);
    rd_i_db_destroy(self->db);

    RDTypeDef** def;
    vect_each(def, &self->typedefs) rd_typedef_destroy(*def);
    vect_destroy(&self->typedefs);

    if(self->processorplugin && self->processorplugin->destroy)
        self->processorplugin->destroy(self->processor);

    rd_i_reader_destroy(self->input_reader);
    rd_i_reader_destroy(self->reader);
    rd_i_buffer_destroy((RDBuffer*)self->input);
    rd_i_strpool_destroy(&self->strings);
    rd_free(self->working_dir);
    rd_free(self->file_name);

    *self = (RDContext){0};
    rd_free(self);
}

RDReader* rd_get_reader(const RDContext* self) { return self->reader; }

RDReader* rd_get_input_reader(const RDContext* self) {
    return self->input_reader;
}

bool rd_read_byte(const RDContext* self, RDAddress address, u8* v) {
    const RDSegmentFull* s = rd_i_db_find_segment(self, address);
    if(!s) return 0;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_read_byte((const RDBuffer*)s->flags, idx, v);
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
            if(!rd_read_byte(ctx, address, &ptr_v)) return false;
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

const char* rd_read_str(const RDContext* self, RDAddress address, usize* n) {
    const RDSegmentFull* s = rd_i_db_find_segment(self, address);
    if(!s) return NULL;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_read_str((RDBuffer*)s->flags, idx, n);
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

bool rd_write_byte(RDContext* self, RDAddress address, u8 v) {
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

RDProblemSlice rd_get_all_problems(const RDContext* self) {
    return vect_to_slice(RDProblemSlice, &self->problems);
}

RDTypeDefSlice rd_get_all_type_defs(const RDContext* self) {
    return (RDTypeDefSlice){
        .data = (const RDTypeDef**)self->typedefs.data,
        .length = self->typedefs.length,
    };
}

RDSegmentSlice rd_get_all_segments(const RDContext* self) {
    const RDSegmentFullVect* segments = rd_i_db_get_segments(self);

    return (RDSegmentSlice){
        .data = (const RDSegment**)segments->data,
        .length = segments->length,
    };
}

RDInputMappingSlice rd_get_all_mappings(const RDContext* self) {
    return vect_to_slice(RDInputMappingSlice, rd_i_db_get_mappings(self));
}

RDAddressSlice rd_get_all_address_by_type(const RDContext* self,
                                          const char* filter) {
    return vect_to_slice(
        RDAddressSlice,
        rd_i_db_get_all_address_by_type(
            (RDContext*)self, (RDAddressVect*)&self->addr_type_buf, filter));
}

RDFunctionSlice rd_get_all_functions(const RDContext* self) {
    return (RDFunctionSlice){
        .data = (const RDFunction**)self->functions.data,
        .length = self->functions.length,
    };
}

RDExternalSlice rd_get_all_externals(const RDContext* self,
                                     RDExternalKind kind) {
    RDExternalVect* v = rd_i_db_get_all_externals(
        (RDContext*)self, kind, (RDExternalVect*)&self->externals);
    return vect_to_slice(RDExternalSlice, v);
}

RDSymbolSlice rd_get_all_symbols(const RDContext* self) {
    RDContext* ctx = (RDContext*)self;
    const RDSegmentFullVect* segments = rd_i_db_get_segments(ctx);
    const RDFunctionVect* functions = &ctx->functions;

    RDAddressVect addresses = {0};
    RDTypeVect types = {0};
    rd_i_db_get_all_types(ctx, &addresses, &types);

    vect_clear(&ctx->symbols);
    vect_reserve(&ctx->symbols, vect_length(segments) + vect_length(functions) +
                                    vect_length(&addresses));

    RDSegmentFull** seg;
    vect_each(seg, segments) {
        vect_push(&ctx->symbols, (RDSymbol){
                                     .kind = RD_SYMBOL_SEGMENT,
                                     .address = (*seg)->base.start_address,
                                     .segment = (const RDSegment*)(*seg),
                                 });
    }

    RDFunction** func;
    vect_each(func, functions) {
        vect_push(&ctx->symbols, (RDSymbol){
                                     .kind = RD_SYMBOL_FUNCTION,
                                     .address = (*func)->address,
                                     .func = *func,
                                 });
    }

    for(usize i = 0; i < vect_length(&addresses); i++) {
        const RDType* t = vect_at(&types, i);

        vect_push(&ctx->symbols, (RDSymbol){
                                     .kind = RD_SYMBOL_TYPE,
                                     .address = *vect_at(&addresses, i),
                                     .type = *t,
                                 });
    }

    vect_clear(&addresses);
    rd_i_db_get_all_name_addresses(ctx, &addresses);

    for(usize i = 0; i < vect_length(&addresses); i++) {
        RDAddress address = *vect_at(&addresses, i);

        const RDSegmentFull* s = rd_i_db_find_segment(ctx, address);
        if(!s) continue;

        usize idx = rd_i_address2index(s, address);
        if(rd_flagsbuffer_has_func(s->flags, idx)) continue;
        if(rd_flagsbuffer_has_type(s->flags, idx)) continue;

        vect_push(&ctx->symbols, (RDSymbol){
                                     .kind = RD_SYMBOL_NAME,
                                     .address = address,
                                 });
    }

    vect_destroy(&types);
    vect_destroy(&addresses);

    vect_sort(&ctx->symbols, rd_i_symbol_sort_pred);
    return vect_to_slice(RDSymbolSlice, &ctx->symbols);
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

bool rd_i_set_external(RDContext* self, const RDExternal* ext) {
    if(!ext) return false;

    if(ext->kind != RD_EXT_EXPORTED && ext->kind != RD_EXT_IMPORTED)
        return false;

    const RDSegmentFull* seg = rd_i_db_find_segment(self, ext->address);

    if(!seg) {
        rd_i_add_problem(self, ext->address, ext->address,
                         "skipping external to unmapped address");
        return false;
    }

    usize idx = rd_i_address2index(seg, ext->address);

    if(rd_flagsbuffer_has_tail(seg->flags, idx)) {
        rd_i_add_problem(self, ext->address, ext->address,
                         "cannot set external on tail byte");
        return false;
    }

    if(ext->kind == RD_EXT_EXPORTED &&
       rd_flagsbuffer_has_imported(seg->flags, idx)) {
        rd_i_add_problem(self, ext->address, ext->address,
                         "already marked as imported, cannot mark as exported");
        return false;
    }

    if(ext->kind == RD_EXT_IMPORTED &&
       rd_flagsbuffer_has_exported(seg->flags, idx)) {
        rd_i_add_problem(self, ext->address, ext->address,
                         "already marked as exported, cannot mark as imported");
        return false;
    }

    if(ext->kind == RD_EXT_EXPORTED &&
       !rd_i_flagsbuffer_set_exported(seg->flags, idx))
        return false;

    if(ext->kind == RD_EXT_IMPORTED &&
       !rd_i_flagsbuffer_set_imported(seg->flags, idx))
        return false;

    rd_i_db_set_external(self, ext);
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
    const RDSegmentFullVect* segments = rd_i_db_get_segments(self);

    if(!vect_is_empty(segments)) {
        if(address) *address = (*vect_first(segments))->base.start_address;

        return true;
    }

    return false;
}

bool rd_set_entry_point(RDContext* self, RDAddress address, const char* name) {
    const RDSegmentFull* seg = rd_i_db_find_segment(self, address);

    if(!seg) {
        rd_i_add_problem(
            self, address, address,
            "trying to set an entry point '%s' to unmapped address",
            name ? name : "<unnamed>");

        return false;
    }

    usize idx = rd_i_address2index(seg, address);

    if(rd_flagsbuffer_has_imported(seg->flags, idx)) {
        rd_i_add_problem(
            self, address, address,
            "trying to set an entry point '%s' to an imported address",
            name ? name : "<unnamed>");
        return false;
    }

    if(self->entry_point.has_value && self->entry_point.value != address) {
        rd_i_add_problem(self, self->entry_point.value, address,
                         "redefining entry point");
    }

    const char* n = name;

    if(!n || !(*n)) {
        const RDLoaderPlugin* ldr = self->loaderplugin;
        assert(ldr && "invalid loader plugin");
        n = rd_i_format(&self->name_buf, "%s_entry_point_%llx", ldr->id,
                        address);
    }

    assert(n && "invalid entry point name");

    rd_library_function(self, address, n);

    // don't override existing exports
    if(!rd_flagsbuffer_has_exported(seg->flags, idx))
        rd_set_external(self, address, NULL, RD_EXT_EXPORTED);

    optional_set(&self->entry_point, address);
    return true;
}

bool rd_get_external(RDContext* self, RDAddress address, RDExternal* ext) {
    return ext && rd_i_db_get_external(self, address, ext);
}

bool rd_get_external_ord(RDContext* self, const char* module, u32 ord,
                         RDExternalKind kind, RDExternal* ext) {
    return ext && rd_i_db_get_external_ord(self, module, ord, kind, ext);
}

bool rd_set_external(RDContext* self, RDAddress address, const char* module,
                     RDExternalKind kind) {
    return rd_i_set_external(self, &(RDExternal){
                                       .kind = kind,
                                       .address = address,
                                       .module = module,
                                       .ordinal = {.has_value = false},
                                   });
}

bool rd_set_external_ord(RDContext* self, RDAddress address, const char* module,
                         u32 ord, RDExternalKind kind) {
    return rd_i_set_external(self,
                             &(RDExternal){
                                 .kind = kind,
                                 .address = address,
                                 .module = module,
                                 .ordinal = {.value = ord, .has_value = true},
                             });
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

void rd_set_min_string(RDContext* self, int l) { self->min_string = l; }

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

bool rd_patch_instruction(RDContext* self, RDAddress address, const char* instr,
                          bool fill_nops) {
    const RDSegmentFull* seg = rd_i_db_find_segment(self, address);

    if(!seg || !(seg->base.perm & RD_SP_X)) {
        RD_LOG_FAIL("cannot patch non-executable address %" PRIx64, address);
        return false;
    }

    usize idx = rd_i_address2index(seg, address), start = idx, end = idx + 1;

    rd_i_flagsbuffer_expand_range(seg->flags, &start, &end);
    address = rd_i_index2address(seg, start);

    RDAddress startaddress = address; // remember start address
    usize maxlen = end - start, len = 0, remaining = 0;
    RDScratchBuffer* buf = rd_scratch_create();

    if(!rd_encode(self, address, instr, buf)) {
        RD_LOG_FAIL("%s", rd_scratch_data(buf));
        goto fail;
    }

    if(rd_scratch_is_empty(buf)) {
        RD_LOG_FAIL("buffer is empty");
        goto fail;
    }

    len = rd_scratch_length(buf);

    if(len > maxlen) { // new instruction is bigger than the previous one
        usize needed_end = start + len;

        while(end < needed_end) {
            end = needed_end;
            rd_i_flagsbuffer_expand_range(seg->flags, &start, &end);
        }

        maxlen = end - start;
    }

    if(!rd_user_undefine_n(self, address, maxlen)) {
        RD_LOG_FAIL("failed to undefine range [%" PRIx64 ", %" PRIx64 ")",
                    address, address + maxlen);
        goto fail;
    }

    if(!rd_write(self, address, rd_scratch_data(buf), len)) {
        RD_LOG_FAIL("failed to write %zu byte(s) at %" PRIx64, len, address);
        goto fail;
    }

    if(!rd_i_flagsbuffer_set_code(seg->flags, rd_i_address2index(seg, address),
                                  len)) {
        RD_LOG_WARN("failed to flag %zu byte(s) at %" PRIx64 " as code", len,
                    address);
    }

    address += len;
    remaining = maxlen - len;

    if(fill_nops && remaining) {
        if(!rd_encode(self, address, NULL, buf)) {
            RD_LOG_FAIL("%s", rd_scratch_data(buf));
            goto fail;
        }

        usize noplen = rd_scratch_length(buf);

        if(!noplen) {
            RD_LOG_FAIL("processor produced an empty NOP");
            goto fail;
        }

        while(remaining >= noplen) {
            if(!rd_write(self, address, rd_scratch_data(buf), noplen)) {
                RD_LOG_FAIL("failed to write NOP at %" PRIx64, address);
                goto fail;
            }

            if(!rd_i_flagsbuffer_set_code(
                   seg->flags, rd_i_address2index(seg, address), len)) {
                RD_LOG_WARN("failed to flag %zu byte(s) at %" PRIx64 " as code",
                            len, address);
            }

            address += noplen;
            remaining -= noplen;
        }

        if(remaining) {
            RD_LOG_WARN("%zu byte(s) at %" PRIx64
                        " left unfilled (smaller than one NOP unit)",
                        remaining, address);
        }
    }

    RDFunction* f = rd_i_find_function(self, address);
    if(f) rd_i_function_rebuild(f);

    for(idx = rd_i_address2index(seg, startaddress);
        idx < rd_i_address2index(seg, address); idx++) {
        if(!rd_flagsbuffer_has_func(seg->flags, idx)) continue;

        RDFunction* curr_f =
            rd_i_find_function(self, rd_i_index2address(seg, idx));
        if(curr_f == f) continue;

        rd_i_function_destroy(curr_f);
        rd_i_flagsbuffer_undefine_func(seg->flags, idx);
    }

    rd_scratch_destroy(buf);
    return true;

fail:
    rd_scratch_destroy(buf);
    return false;
}

void rd_set_scan_char16(RDContext* self, bool b) { self->scan_char16 = b; }

const char* rd_get_working_dir(const RDContext* self) {
    return self->working_dir;
}

const char* rd_get_file_name(const RDContext* self) { return self->file_name; }

const char* rd_str_intern(RDContext* self, const char* s) {
    return rd_i_strpool_intern(&self->strings, s);
}

RDAddressSpace rd_get_address_space(const RDContext* ctx) {
    const RDSegmentFullVect* segments = rd_i_db_get_segments(ctx);
    if(vect_is_empty(segments)) return (RDAddressSpace){0};

    const RDSegmentFull* first = *vect_first(segments);
    const RDSegmentFull* last = *vect_last(segments);

    return (RDAddressSpace){
        .start = first->base.start_address,
        .end = last->base.end_address,
        .size = last->base.end_address - first->base.start_address,
    };
}
