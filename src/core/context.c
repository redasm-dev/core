#include "context.h"
#include "core/engine.h"
#include "core/mapping.h"
#include "core/segment.h"
#include "core/stringfinder.h"
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RD_WORKER_QUEUE_SIZE 8192

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
    RDContext* self = calloc(1, sizeof(*self));

    self->loaderplugin = lplugin, self->loader = ldr;
    self->filepath = rd_strdup(filepath);
    self->input = input;
    self->input_reader = rd_i_reader_create((RDBuffer*)input);
    self->reader = rd_i_reader_create_flags(self);
    self->min_string = RD_MIN_STRING_LENGTH;

    rd_i_listing_init(&self->listing, NULL);
    rd_i_register_primitives(self);
    rd_i_db_init(self);

    queue_reserve(&self->engine.qcall, RD_WORKER_QUEUE_SIZE);
    queue_reserve(&self->engine.qjump, RD_WORKER_QUEUE_SIZE);
    return self;
}

bool rd_i_get_name(RDContext* self, RDAddress address, bool autoname,
                   RDName* n) {
    const RDSegmentFull* seg = rd_i_find_segment(self, address);
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
    const RDSegmentFull* seg = rd_i_find_segment(self, address);
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

const RDProcessorPlugin* rd_get_processor_plugin(const RDContext* self) {
    return self->processorplugin;
}

RDAnalyzerItemSlice rd_get_analyzer_plugins(const RDContext* self) {
    return vect_to_slice(RDAnalyzerItemSlice, &self->analyzerplugins);
}

RDLoader* rd_get_loader(const RDContext* self) { return self->loader; }
RDProcessor* rd_get_processor(const RDContext* self) { return self->processor; }

bool rd_map_segment(RDContext* self, const char* name, RDAddress addr,
                    RDAddress endaddr, u32 perm) {
    if(addr >= endaddr) return false;

    // check for overlaps with existing segments
    RDSegmentFull** it;
    vect_each(it, &self->segments) {
        RDAddress s = (*it)->base.start_address;
        RDAddress e = (*it)->base.end_address;
        if(addr < e && endaddr > s) {

            rd_i_add_problem(
                self, addr, addr,
                "segment '%s' [%llx, %llx) overlaps with '%s' [%llx, %llx)",
                name, addr, endaddr, (*it)->base.name, s, e);
            return false;
        }
    }

    LOG_INFO("mapping segment '%s' (Address: [%X, %X))", name, addr, endaddr);

    // TODO: davide - Store flags for this segment in DB
    RDSegmentFull* s = calloc(1, sizeof(*s));
    s->base.name = rd_i_strpool_intern(&self->strings, name);
    s->base.start_address = addr;
    s->base.end_address = endaddr;
    s->base.perm = perm;
    s->flags = rd_i_flagsbuffer_create(endaddr - addr);

    vect_push(&self->segments, s);
    rd_i_db_add_segment(self, s);
    vect_sort(&self->segments, rd_i_segment_cmp_pred);
    return true;
}

bool rd_map_segment_n(RDContext* self, const char* name, RDAddress addr,
                      usize n, u32 perm) {
    return rd_map_segment(self, name, addr, addr + n, perm);
}

bool rd_map_input(RDContext* self, RDOffset off, RDAddress addr,
                  RDOffset endaddr) {
    RDAddress endoff = endaddr - addr + off;
    if(off >= endoff || addr >= endaddr) return false;

    usize n = endaddr - addr;
    if(off + n > self->input->base.length) return false;

    // segment must exist and mapping must not cross boundary
    const RDSegmentFull* seg = rd_i_find_segment(self, addr);

    if(!seg) {
        rd_i_add_problem(self, addr, addr, "no segment at %llx", addr);
        return false;
    }

    if(endaddr > seg->base.end_address) {
        rd_i_add_problem(
            self, addr, addr,
            "[%llx, %llx) crosses segment boundary '%s' [%llx, %llx)", addr,
            endaddr, seg->base.name, seg->base.start_address,
            seg->base.end_address);
        return false;
    }

    // overlap check against existing mappings
    RDInputMapping* it;
    vect_each(it, &self->mappings) {
        if(addr < it->end_address && endaddr > it->start_address) {
            rd_i_add_problem(
                self, addr, addr,
                "[%llx, %llx) overlaps with existing mapping [%llx, %llx)",
                addr, endaddr, it->start_address, it->end_address);
            return false;
        }
    }

    LOG_INFO("mapping input at %X (Address: [%X, %X))", off, addr, endaddr);

    // write bytes into the segment's flags buffer
    usize buf_idx = rd_i_address2index(seg, addr);
    rd_i_buffer_write((RDBuffer*)seg->flags, buf_idx, self->input->data + off,
                      n);

    RDInputMapping m = {
        .offset = off,
        .start_address = addr,
        .end_address = endaddr,
    };

    vect_push(&self->mappings, m);
    rd_i_db_add_mapping(self, &m);
    vect_sort(&self->mappings, rd_i_mapping_cmp_pred);
    return true;
}

bool rd_map_input_n(RDContext* self, RDOffset off, RDAddress addr, usize n) {
    return rd_map_input(self, off, addr, addr + n);
}

const RDSegmentFull* rd_i_find_segment(const RDContext* self, RDAddress addr) {
    RDSegmentFull** res = (RDSegmentFull**)vect_bsearch(&addr, &self->segments,
                                                        rd_i_segment_find_pred);
    return res ? *res : NULL;
}

const RDSegment* rd_find_segment(const RDContext* self, RDAddress addr) {
    return (const RDSegment*)rd_i_find_segment(self, addr);
}

const RDFunction* rd_find_function(const RDContext* self, RDAddress address) {
    const RDFunctionChunkVect* chunks = &self->listing.chunks;
    if(vect_is_empty(chunks)) return NULL;

    const RDFunctionChunk** chunk = (const RDFunctionChunk**)vect_bsearch(
        &address, chunks, rd_i_function_find_chunk_pred);
    if(!chunk) return NULL;

    const RDFunction** f = (const RDFunction**)vect_bsearch(
        &(*chunk)->func_address, &self->listing.functions,
        rd_i_function_find_pred);
    return f ? *f : NULL;
}

bool rd_to_offset(const RDContext* self, RDAddress address, RDOffset* offset) {
    RDInputMapping* m = (RDInputMapping*)vect_bsearch(&address, &self->mappings,
                                                      rd_i_mapping_find_pred);
    if(!m) return false;
    if(offset) *offset = (address - m->start_address) + m->offset;
    return true;
}

bool rd_to_address(const RDContext* self, RDOffset offset, RDAddress* address) {
    RDInputMapping* m;

    vect_each(m, &self->mappings) {
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

bool rd_get_address(RDContext* self, const char* name, RDAddress* address) {
    if(!name) return false;
    if(rd_i_db_get_address(self, name, address)) return true;

    // check if it's an autogenerated name (format prefix_<hex>)
    const char* uscore = strrchr(name, '_');
    if(!uscore || uscore == name) return false;

    const char* s = uscore + 1;
    if(*s == 0) return false;

    for(const char* p = s; *p; p++) {
        if(!isxdigit((unsigned char)*p)) return false;
    }

    errno = 0;
    RDAddress val = strtoull(s, NULL, 16);
    panic_if(errno != 0, "rd_get_address: %s", strerror(errno));
    if(address) *address = val;

    return true;
}

const char* rd_get_comment(RDContext* self, RDAddress address) {
    const RDSegmentFull* seg = rd_i_find_segment(self, address);
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

const char* rd_to_hex(const RDContext* self, usize v) {
    assert(self->processorplugin && "invalid processor plugin");
    return rd_i_to_hex((isize)v, self->processorplugin->ptr_size);
}

const char* rd_render_text(RDContext* self, RDAddress address) {
    LIndex idx = rd_i_listing_lower_bound(&self->listing, address);

    if(idx < vect_length(&self->listing)) {
        // find first code item
        while(idx < vect_length(&self->listing) &&
              vect_at(&self->listing, idx)->address == address) {
            if(vect_at(&self->listing, idx)->kind == RD_LK_INSTRUCTION) break;
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

bool rd_undefine(RDContext* ctx, RDAddress address) {
    const RDSegmentFull* seg = rd_i_find_segment(ctx, address);
    if(!seg) return false;

    usize idx = rd_i_address2index(seg, address);
    usize len = rd_i_flagsbuffer_get_range_length(seg->flags, idx);

    rd_i_flagsbuffer_undefine(seg->flags, idx, len);
    rd_i_db_del_type(ctx, address); // remove type link from DB if any
    return true;
}

RDDelaySlotInfo rd_get_delay_slot_info(const RDContext* self) {
    return self->engine.dslot_info;
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
    const RDSegmentFull* seg = rd_i_find_segment(self, address);
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
    const RDSegmentFull* seg = rd_i_find_segment(self, address);
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

bool rd_i_set_function(RDContext* self, RDAddress address, const char* name,
                       RDConfidence c) {
    const RDSegmentFull* seg = rd_i_find_segment(self, address);
    if(!seg || !(seg->base.perm & RD_SP_X)) return false;

    usize index = rd_i_address2index(seg, address);

    // conflict checks
    if(rd_flagsbuffer_has_tail(seg->flags, index)) return false;
    if(rd_flagsbuffer_has_data(seg->flags, index)) return false;
    return rd_i_engine_enqueue_call(self, address, name, c);
}

void rd_destroy(RDContext* self) {
    if(!self) return;

    vect_destroy(&self->problem_buf);
    vect_destroy(&self->sym_buf);
    vect_destroy(&self->str_buf);
    vect_destroy(&self->imp_buf);
    vect_destroy(&self->name_buf);
    vect_destroy(&self->problems);
    vect_destroy(&self->xrefs_to);
    vect_destroy(&self->xrefs_to_type);
    vect_destroy(&self->xrefs_from);
    vect_destroy(&self->xrefs_from_type);
    vect_destroy(&self->mappings);
    rd_i_il_deinit(&self->il_buf);
    rd_i_listing_deinit(&self->listing);
    rd_i_engine_destroy(self);

    RDSegmentFull** s;
    vect_each(s, &self->segments) {
        rd_i_buffer_destroy((RDBuffer*)(*s)->flags);
        free(*s);
    }

    vect_destroy(&self->segments);
    rd_i_db_deinit(self);

    RDTypeDef** def;
    vect_each(def, &self->types) rd_i_typedef_destroy(*def);
    vect_destroy(&self->types);

    RDAnalyzerItem** ai;
    vect_each(ai, &self->analyzerplugins) free(*ai);
    vect_destroy(&self->analyzerplugins);

    if(self->processorplugin && self->processorplugin->destroy)
        self->processorplugin->destroy(self->processor);

    if(self->loaderplugin && self->loaderplugin->destroy)
        self->loaderplugin->destroy(self->loader);

    rd_i_reader_destroy(self->input_reader);
    rd_i_reader_destroy(self->reader);
    rd_i_buffer_destroy((RDBuffer*)self->input);
    rd_i_strpool_destroy(&self->strings);
    free(self->filepath);
    free(self);
}

RDReader* rd_get_reader(const RDContext* self) { return self->reader; }

RDReader* rd_get_input_reader(const RDContext* self) {
    return self->input_reader;
}

bool rd_read_u8(const RDContext* self, RDAddress address, u8* v) {
    const RDSegmentFull* s = rd_i_find_segment(self, address);
    if(!s) return 0;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_read_u8((const RDBuffer*)s->flags, idx, v);
}

bool rd_read_le16(const RDContext* self, RDAddress address, u16* v) {
    const RDSegmentFull* s = rd_i_find_segment(self, address);
    if(!s) return 0;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_read_le16((const RDBuffer*)s->flags, idx, v);
}

bool rd_read_le32(const RDContext* self, RDAddress address, u32* v) {
    const RDSegmentFull* s = rd_i_find_segment(self, address);
    if(!s) return 0;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_read_le32((const RDBuffer*)s->flags, idx, v);
}

bool rd_read_le64(const RDContext* self, RDAddress address, u64* v) {
    const RDSegmentFull* s = rd_i_find_segment(self, address);
    if(!s) return 0;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_read_le64((const RDBuffer*)s->flags, idx, v);
}

bool rd_read_be16(const RDContext* self, RDAddress address, u16* v) {
    const RDSegmentFull* s = rd_i_find_segment(self, address);
    if(!s) return 0;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_read_be16((const RDBuffer*)s->flags, idx, v);
}

bool rd_read_be32(const RDContext* self, RDAddress address, u32* v) {
    const RDSegmentFull* s = rd_i_find_segment(self, address);
    if(!s) return false;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_read_be32((const RDBuffer*)s->flags, idx, v);
}

bool rd_read_be64(const RDContext* self, RDAddress address, u64* v) {
    const RDSegmentFull* s = rd_i_find_segment(self, address);
    if(!s) return false;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_read_be64((const RDBuffer*)s->flags, idx, v);
}

bool rd_expect_u8(const RDContext* self, RDAddress address, u8 v) {
    const RDSegmentFull* s = rd_i_find_segment(self, address);
    if(!s) return false;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_expect_u8((const RDBuffer*)s->flags, idx, v);
}

bool rd_expect_le16(const RDContext* self, RDAddress address, u16 v) {
    const RDSegmentFull* s = rd_i_find_segment(self, address);
    if(!s) return false;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_expect_le16((const RDBuffer*)s->flags, idx, v);
}

bool rd_expect_le32(const RDContext* self, RDAddress address, u32 v) {
    const RDSegmentFull* s = rd_i_find_segment(self, address);
    if(!s) return false;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_expect_le32((const RDBuffer*)s->flags, idx, v);
}

bool rd_expect_le64(const RDContext* self, RDAddress address, u64 v) {
    const RDSegmentFull* s = rd_i_find_segment(self, address);
    if(!s) return false;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_expect_le64((const RDBuffer*)s->flags, idx, v);
}

bool rd_expect_be16(const RDContext* self, RDAddress address, u16 v) {
    const RDSegmentFull* s = rd_i_find_segment(self, address);
    if(!s) return false;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_expect_be16((const RDBuffer*)s->flags, idx, v);
}

bool rd_expect_be32(const RDContext* self, RDAddress address, u32 v) {
    const RDSegmentFull* s = rd_i_find_segment(self, address);
    if(!s) return false;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_expect_be32((const RDBuffer*)s->flags, idx, v);
}

bool rd_expect_be64(const RDContext* self, RDAddress address, u64 v) {
    const RDSegmentFull* s = rd_i_find_segment(self, address);
    if(!s) return false;
    usize idx = rd_i_address2index(s, address);
    return rd_i_buffer_expect_be64((const RDBuffer*)s->flags, idx, v);
}

usize rd_read(const RDContext* self, RDAddress address, void* data, usize n) {
    const RDSegmentFull* s = rd_i_find_segment(self, address);
    if(!s) return false;
    return rd_i_buffer_read((RDBuffer*)s->flags, rd_i_address2index(s, address),
                            data, n);
}

RDProblemSlice rd_get_all_problems(const RDContext* self) {
    return vect_to_slice(RDProblemSlice, &self->problems);
}

RDSegmentSlice rd_get_all_segments(const RDContext* self) {
    return (RDSegmentSlice){
        .data = (const RDSegment**)self->segments.data,
        .length = self->segments.length,
    };
}

RDInputMappingSlice rd_get_all_mappings(const RDContext* self) {
    return vect_to_slice(RDInputMappingSlice, &self->mappings);
}

RDFunctionSlice rd_get_all_functions(const RDContext* self) {
    return (RDFunctionSlice){
        .data = (const RDFunction**)self->listing.functions.data,
        .length = self->listing.functions.length,
    };
}

RDAddressSlice rd_get_all_exported(const RDContext* self) {
    return vect_to_slice(RDAddressSlice, &self->listing.exported);
}

RDAddressSlice rd_get_all_imported(const RDContext* self) {
    return vect_to_slice(RDAddressSlice, &self->listing.imported);
}

RDSymbolSlice rd_get_all_symbols(const RDContext* self) {
    return vect_to_slice(RDSymbolSlice, &self->listing.symbols);
}

const RDXRefVect* rd_i_get_xrefs_from_ex(RDContext* self, RDAddress fromaddr,
                                         RDXRefVect* r) {
    vect_clear(r);

    const RDSegmentFull* seg = rd_i_find_segment(self, fromaddr);
    if(!seg) return r;

    usize idx = rd_i_address2index(seg, fromaddr);
    if(!rd_i_flagsbuffer_has_xref_out(seg->flags, idx)) return r;

    rd_i_db_get_xrefs_from(self, fromaddr, r);
    return r;
}

const RDXRefVect* rd_i_get_xrefs_from_type_ex(RDContext* self,
                                              RDAddress fromaddr, usize type,
                                              RDXRefVect* r) {
    vect_clear(r);

    const RDSegmentFull* seg = rd_i_find_segment(self, fromaddr);
    if(!seg) return r;

    usize idx = rd_i_address2index(seg, fromaddr);
    if(!rd_i_flagsbuffer_has_xref_out(seg->flags, idx)) return r;

    rd_i_db_get_xrefs_from_type(self, fromaddr, type, r);
    return r;
}

const RDXRefVect* rd_i_get_xrefs_to_ex(RDContext* self, RDAddress toaddr,
                                       RDXRefVect* r) {
    vect_clear(r);

    const RDSegmentFull* seg = rd_i_find_segment(self, toaddr);
    if(!seg) return r;

    usize idx = rd_i_address2index(seg, toaddr);
    if(!rd_i_flagsbuffer_has_xref_in(seg->flags, idx)) return r;

    rd_i_db_get_xrefs_to(self, toaddr, r);
    return r;
}

const RDXRefVect* rd_i_get_xrefs_to_type_ex(RDContext* self, RDAddress toaddr,
                                            usize type, RDXRefVect* r) {
    vect_clear(r);

    const RDSegmentFull* seg = rd_i_find_segment(self, toaddr);
    if(!seg) return r;

    usize idx = rd_i_address2index(seg, toaddr);
    if(!rd_i_flagsbuffer_has_xref_in(seg->flags, idx)) return r;

    rd_i_db_get_xrefs_to_type(self, toaddr, type, r);
    return r;
}

const RDXRefVect* rd_i_get_xrefs_from(RDContext* self, RDAddress fromaddr) {
    return rd_i_get_xrefs_from_ex(self, fromaddr, &self->xrefs_from);
}

const RDXRefVect* rd_i_get_xrefs_from_type(RDContext* self, RDAddress fromaddr,
                                           usize type) {
    return rd_i_get_xrefs_from_type_ex(self, fromaddr, type,
                                       &self->xrefs_from_type);
}

const RDXRefVect* rd_i_get_xrefs_to(RDContext* self, RDAddress toaddr) {
    return rd_i_get_xrefs_to_ex(self, toaddr, &self->xrefs_to);
}

const RDXRefVect* rd_i_get_xrefs_to_type(RDContext* self, RDAddress toaddr,
                                         usize type) {
    return rd_i_get_xrefs_to_type_ex(self, toaddr, type, &self->xrefs_to_type);
}

const char* rd_symbol_to_string(const RDSymbol* self, RDContext* ctx) {
    switch(self->kind) {
        case RD_SYMBOL_SEGMENT: {
            const RDSegmentFull* seg = rd_i_find_segment(ctx, self->address);
            return seg ? seg->base.name : NULL;
        }

        case RD_SYMBOL_STRING: {
            const RDSegmentFull* seg = rd_i_find_segment(ctx, self->address);
            assert(seg &&
                   "cannot convert symbol to string, type outside of segments");

            RDTypeFull t;
            bool ok = rd_i_get_type(ctx, self->address, &t);
            assert(ok && "cannot convert symbol to string, type not found");

            // clang-format off
            // reserve at least these bytes, +2 for quoting, +1 null terminator
            usize sz = rd_i_size_of(ctx, t.base.name, t.base.count,
                                          t.base.flags) + 3;
            // clang-format on

            usize idx = rd_i_address2index(seg, self->address);
            vect_reserve(&ctx->sym_buf, sz);
            vect_clear(&ctx->sym_buf);
            vect_push(&ctx->sym_buf, '\"');

            for(usize i = 0; i < t.base.count; i++) {
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
    const RDSegmentFull* seg = rd_i_find_segment(self, address);
    if(!seg) return false;

    usize idx = rd_i_address2index(seg, address);

    if(rd_flagsbuffer_has_tail(seg->flags, idx)) {
        rd_i_add_problem(self, address, address,
                         "cannot set imported on tail byte");
        return false;
    }

    if(name) {
        if(imp->module)
            name = rd_i_format(&self->imp_buf, "%s_%s", imp->module, name);

        rd_i_set_name(self, address, name, RD_CONFIDENCE_LIBRARY);
    }

    const char* ptrtype =
        rd_integral_from_size(self->processorplugin->ptr_size);

    rd_i_set_type(self, address, ptrtype, 0, RD_TYPE_ISPOINTER,
                  RD_CONFIDENCE_AUTO);

    rd_i_flagsbuffer_set_imported(seg->flags, idx);
    rd_i_db_set_imported(self, address, imp);
    return true;
}

bool rd_get_entry_point(const RDContext* self, RDAddress* address) {
    if(self->entry_point.has_value) {
        if(address) *address = self->entry_point.value;
        return true;
    }

    // try to fallback to the first address available
    if(!vect_is_empty(&self->segments)) {
        if(address)
            *address = (*vect_front(&self->segments))->base.start_address;

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

bool rd_set_exported(RDContext* ctx, RDAddress address, const char* name) {
    const RDSegmentFull* seg = rd_i_find_segment(ctx, address);
    if(!seg) return false;

    usize idx = rd_i_address2index(seg, address);

    if(rd_flagsbuffer_has_tail(seg->flags, idx)) {
        rd_i_add_problem(ctx, address, address,
                         "cannot set exported on tail byte");
        return false;
    }

    if(name) rd_i_set_name(ctx, address, name, RD_CONFIDENCE_LIBRARY);
    return rd_i_flagsbuffer_set_exported(seg->flags, idx);
}

bool rd_set_imported(RDContext* ctx, RDAddress address, const char* module,
                     const char* name) {
    return rd_i_set_imported(ctx, address, name,
                             &(RDImported){
                                 .module = module,
                                 .ordinal = {.has_value = false},
                             });
}

bool rd_set_imported_ord(RDContext* ctx, RDAddress address, const char* module,
                         const char* name, u64 ord) {
    return rd_i_set_imported(ctx, address, name,
                             &(RDImported){
                                 .module = module,
                                 .ordinal = {.value = ord, .has_value = true},
                             });
}

bool rd_get_imported(RDContext* ctx, RDAddress address, RDImported* imp) {
    const RDSegmentFull* seg = rd_i_find_segment(ctx, address);
    if(!seg) return false;

    usize idx = rd_i_address2index(seg, address);
    if(!rd_flagsbuffer_has_imported(seg->flags, idx)) return false;
    return rd_i_db_get_imported(ctx, address, imp);
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
                 usize type) {
    const RDSegmentFull* fromseg = rd_i_find_segment(self, fromaddr);
    const RDSegmentFull* toseg = rd_i_find_segment(self, toaddr);
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

    switch(type) {
        case RD_DR_READ:
        case RD_DR_WRITE:
        case RD_DR_ADDRESS: {
            rd_i_db_add_xref(self, fromaddr, toaddr, type);
            rd_i_flagsbuffer_set_xref_out(fromseg->flags, fromidx);

            if(!isreftotail) rd_i_flagsbuffer_set_xref_in(toseg->flags, toidx);
            break;
        }

        case RD_CR_JUMP: {
            rd_i_db_add_xref(self, fromaddr, toaddr, type);
            rd_i_flagsbuffer_set_xref_out(fromseg->flags, fromidx);

            if(!isreftotail) {
                rd_i_flagsbuffer_set_xref_in(toseg->flags, toidx);
                rd_i_engine_enqueue_jump(self, toaddr);
            }

            break;
        }

        case RD_CR_CALL: {
            rd_i_db_add_xref(self, fromaddr, toaddr, type);
            rd_i_flagsbuffer_set_xref_out(fromseg->flags, fromidx);

            if(!isreftotail) {
                rd_i_flagsbuffer_set_xref_in(toseg->flags, toidx);
                rd_i_engine_enqueue_call(self, toaddr, NULL,
                                         RD_CONFIDENCE_AUTO);
            }

            break;
        }

        default: return false;
    }

    return true;
}

RDXRefSlice rd_get_xrefs_from(RDContext* self, RDAddress fromaddr) {
    const RDXRefVect* refs = rd_i_get_xrefs_from(self, fromaddr);
    return vect_to_slice(RDXRefSlice, refs);
}

RDXRefSlice rd_get_xrefs_from_type(RDContext* self, RDAddress fromaddr,
                                   usize type) {
    const RDXRefVect* refs = rd_i_get_xrefs_from_type(self, fromaddr, type);
    return vect_to_slice(RDXRefSlice, refs);
}

RDXRefSlice rd_get_xrefs_to(RDContext* self, RDAddress toaddr) {
    const RDXRefVect* refs = rd_i_get_xrefs_to(self, toaddr);
    return vect_to_slice(RDXRefSlice, refs);
}

RDXRefSlice rd_get_xrefs_to_type(RDContext* self, RDAddress toaddr,
                                 usize type) {
    const RDXRefVect* refs = rd_i_get_xrefs_to_type(self, toaddr, type);
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
