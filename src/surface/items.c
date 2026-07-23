#include "items.h"
#include "core/context.h"
#include "io/flagsbuffer.h"
#include "support/containers.h"
#include "support/error.h"
#include <ctype.h>
#include <inttypes.h>

#define RD_SURFACE_WS_COMMENT 8
#define RD_SURFACE_WS_REFS 8

static void _rd_render_modifiers(RDRenderer* r, RDAddress address,
                                 RDThemeKind fg, RDThemeKind bg) {
    const RDSegmentFull* seg = rd_i_renderer_find_segment(r, address);
    usize idx = rd_i_address2index(seg, address);

    if(rd_flagsbuffer_has_imported(seg->flags, idx))
        rd_renderer_text(r, "imported ", fg, bg);
    else if(rd_flagsbuffer_has_exported(seg->flags, idx))
        rd_renderer_text(r, "exported ", fg, bg);
}

static void _rd_render_value(RDRenderer* r, RDAddress address, const RDType* t,
                             bool term) {
    const RDSegmentFull* seg = rd_i_db_find_segment(r->context, address);
    panic_if(!seg, "_rd_render_value: invalid segment");

    const RDBuffer* flags = (const RDBuffer*)seg->flags;
    const unsigned int PTR_SIZE = rd_get_ptr_size(r->context);
    const unsigned int CPTR_SIZE = rd_get_code_ptr_size(r->context);

    bool is_be = r->context->processorplugin->flags & RD_PF_BE;
    usize idx = rd_i_address2index(seg, address);

    // pointer
    const char* ptr_type = NULL;
    unsigned int calc_ptr_size = 0;

    if(t->mod == RD_TYPE_PTR) {
        ptr_type = rd_integral_from_size(PTR_SIZE);
        calc_ptr_size = PTR_SIZE;
    }
    else if(t->mod == RD_TYPE_CPTR) {
        ptr_type = rd_integral_from_size(CPTR_SIZE);
        calc_ptr_size = CPTR_SIZE;
    }

    if(ptr_type) {
        u64 v;
        if(rd_i_buffer_read_primitive(flags, idx, ptr_type, is_be, &v)) {
            const unsigned int F = calc_ptr_size * 2;
            rd_renderer_loc(r, v, F, RD_NUM_DEFAULT);
        }
        else
            rd_renderer_muted(r, "?");

        return;
    }

    // char array - render as string
    if(!strcmp(t->def->name, "char") && t->count > 0) {
        rd_renderer_text(r, "\"", RD_THEME_STRING, RD_THEME_BACKGROUND);
        usize i = 0;
        for(; i < t->count - 1; i++) {
            u8 v;

            if(!rd_i_buffer_read_byte(flags, idx + i, &v)) {
                rd_renderer_muted(r, "?");
                continue;
            }

            if(!v) break;

            rd_renderer_text(r, rd_i_escape_char((char)v, true),
                             RD_THEME_STRING, RD_THEME_BACKGROUND);
        }
        rd_renderer_text(r, "\"", RD_THEME_STRING, RD_THEME_BACKGROUND);

        if(term) { // render string terminator
            rd_renderer_norm(r, ",");
            u8 v;

            if(rd_i_buffer_read_byte(flags, idx + i, &v)) {
                if(v) {
                    rd_renderer_text(r, rd_i_escape_char((char)v, true),
                                     RD_THEME_MUTED, RD_THEME_BACKGROUND);
                }
                else
                    rd_renderer_num(r, 0, 10, 0, RD_NUM_DEFAULT);
            }
            else
                rd_renderer_muted(r, "?");
        }
        return;
    }

    // char16 array - render as string
    if(!strcmp(t->def->name, "char16") && t->count > 0) {
        rd_renderer_text(r, "\"", RD_THEME_STRING, RD_THEME_BACKGROUND);
        for(usize i = 0; i < t->count; i++) {
            bool ok = false;
            u16 v;

            if(is_be)
                ok = rd_i_buffer_read_be16(flags, idx + (i * sizeof(i16)), &v);
            else
                ok = rd_i_buffer_read_le16(flags, idx + (i * sizeof(i16)), &v);

            if(!ok) {
                rd_renderer_muted(r, "?");
                continue;
            }

            if(!v) break;

            rd_renderer_text(r, rd_i_escape_char16(v, true), RD_THEME_STRING,
                             RD_THEME_BACKGROUND);
        }
        rd_renderer_text(r, "\"", RD_THEME_STRING, RD_THEME_BACKGROUND);
        if(term) {
            rd_renderer_norm(r, ",");
            rd_renderer_num(r, 0, 10, 0, RD_NUM_DEFAULT);
        }
        return;
    }

    // single char
    if(!strcmp(t->def->name, "char")) {
        u8 v;
        rd_renderer_norm(r, "'");
        if(rd_i_buffer_read_byte(flags, idx, &v))
            rd_renderer_text(r, rd_i_escape_char((char)v, false),
                             RD_THEME_STRING, RD_THEME_BACKGROUND);
        else
            rd_renderer_muted(r, "?");
        rd_renderer_norm(r, "'");
        return;
    }

    // single char16
    if(!strcmp(t->def->name, "char16")) {
        u16 v;
        rd_renderer_norm(r, "'");

        bool ok = is_be ? rd_i_buffer_read_be16(flags, idx, &v)
                        : rd_i_buffer_read_le16(flags, idx, &v);

        if(ok)
            rd_renderer_text(r, rd_i_escape_char16(v, false), RD_THEME_STRING,
                             RD_THEME_BACKGROUND);
        else
            rd_renderer_muted(r, "?");

        rd_renderer_norm(r, "'");
        return;
    }

    if(t->count > 0) return; // non-string array, ignore

    // numeric primitive
    u64 v;
    if(rd_i_buffer_read_primitive(flags, idx, t->def->name, is_be, &v)) {
        unsigned int sz =
            (unsigned int)rd_i_size_of(r->context, t->def->name, 0, t->mod);
        panic_if(!sz, "type '%s' has unresolved size", t->def->name);

        if(sz == PTR_SIZE)
            rd_renderer_loc(r, (RDAddress)v, sz * 2, RD_NUM_DEFAULT);
        else
            rd_renderer_num(r, (i64)v, 16, sz * 2, RD_NUM_DEFAULT);
    }
    else
        rd_renderer_muted(r, "?");
}

static void _rd_render_refs(RDRenderer* r, RDAddress address) {
    if(rd_i_renderer_has_flag(r, RD_RF_NO_REFS)) return;

    RDContext* ctx = r->context;
    if(!rd_i_get_xrefs_from_ex(ctx, address, RD_XR_NONE, &r->xrefs)) return;

    const RDXRef* xref;
    vect_each(xref, &r->xrefs) {
        const RDSegmentFull* seg = rd_i_db_find_segment(ctx, xref->address);
        if(!seg) continue;

        RDTypeFull t;
        if(!rd_i_get_type(ctx, xref->address, &t)) continue;

        bool is_ptr = rd_type_is_ptr(&t.base);
        RDAddress ptr_address = 0, xref_address = xref->address;

        // try to follow the pointer location
        if(is_ptr && xref->type == RD_DR_READ &&
           rd_follow_ptr(ctx, xref->address, &ptr_address)) {
            seg = rd_i_db_find_segment(ctx, ptr_address);

            bool has_xrefs_in =
                seg && rd_i_flagsbuffer_has_xref_in(
                           seg->flags, rd_i_address2index(seg, ptr_address));

            if(has_xrefs_in && rd_i_get_type(ctx, ptr_address, &t))
                xref_address = ptr_address;
            else
                is_ptr = false;
        }

        // render strings only
        if((strcmp(t.base.def->name, "char") != 0 &&
            strcmp(t.base.def->name, "char16") != 0) ||
           !t.base.count)
            continue;

        rd_renderer_ws(r, RD_SURFACE_WS_REFS);
        if(is_ptr) rd_renderer_norm(r, " => ");
        _rd_render_value(r, xref_address, &t.base, false);
    }
}

static void _rd_render_comment(RDRenderer* r, RDAddress address) {
    if(rd_i_renderer_has_flag(r, RD_RF_NO_COMMENTS)) return;

    const char* cmt = rd_get_comment(r->context, address);
    if(!cmt) return;

    vect_clear(&r->comment_buf);

    rd_renderer_ws(r, RD_SURFACE_WS_COMMENT);
    rd_renderer_text(r, "# ", RD_THEME_COMMENT, RD_THEME_BACKGROUND);

    for(const char* c = cmt; *c; c++) {
        if(*c == '\n') {
            if(vect_is_empty(&r->comment_buf)) continue;

            vect_push(&r->comment_buf, 0);
            rd_renderer_text(r, r->comment_buf.data, RD_THEME_COMMENT,
                             RD_THEME_BACKGROUND);

            vect_clear(&r->comment_buf);
            rd_renderer_text(r, " | ", RD_THEME_COMMENT, RD_THEME_BACKGROUND);
        }
        else
            vect_push(&r->comment_buf, *c);
    }

    if(!vect_is_empty(&r->comment_buf)) {
        vect_push(&r->comment_buf, 0);
        rd_renderer_text(r, r->comment_buf.data, RD_THEME_COMMENT,
                         RD_THEME_BACKGROUND);
    }
}

static void _rd_render_comment_item(RDRenderer* r, const RDSegmentFull* seg,
                                    usize idx, usize sub_line,
                                    const char* comment) {
    if(rd_i_renderer_has_flag(r, RD_RF_NO_COMMENTS)) return;

    rd_i_renderer_new_row(r, seg, idx, sub_line, 8);
    rd_renderer_text(r, "<", RD_THEME_MUTED, RD_THEME_BACKGROUND);
    rd_renderer_text(r, comment, RD_THEME_MUTED, RD_THEME_BACKGROUND);
    rd_renderer_text(r, ">", RD_THEME_MUTED, RD_THEME_BACKGROUND);
}

static void _rd_render_label_item(RDRenderer* r, const RDSegmentFull* seg,
                                  usize idx, usize sub_line) {
    RDAddress address = rd_i_renderer_new_row(r, seg, idx, sub_line, 6);

    RDName n;
    bool hasname = rd_i_get_name(r->context, address, true, &n);
    assert(hasname && "cannot get label name");

    rd_renderer_text(r, n.value, RD_THEME_LOCATION, RD_THEME_BACKGROUND);
    rd_renderer_text(r, ":", RD_THEME_LOCATION, RD_THEME_BACKGROUND);
}

static void _rd_render_function_item(RDRenderer* r, const RDSegmentFull* seg,
                                     usize idx, usize sub_line) {
    if(rd_i_renderer_has_flag(r, RD_RF_NO_FUNCTION)) return;

    const RDProcessorPlugin* p = r->context->processorplugin;
    RDAddress address = rd_i_renderer_new_row(r, seg, idx, sub_line, 4);

    const RDFunction* f = rd_i_find_function(r->context, address);
    panic_if(!f, "function not found @ %x", address);

    if(p->render_function) {
        p->render_function(r, f, r->context->processor);
        return;
    }

    _rd_render_modifiers(r, address, RD_THEME_FUNCTION, RD_THEME_BACKGROUND);
    const char* func_str = rd_i_function_to_str(f, r->context);

    if(func_str) {
        rd_renderer_text(r, func_str, RD_THEME_FUNCTION, RD_THEME_BACKGROUND);
        return;
    }

    RDName n;
    bool hasname = rd_i_get_name(r->context, address, true, &n);
    assert(hasname);

    rd_renderer_text(r, "function ", RD_THEME_FUNCTION, RD_THEME_BACKGROUND);
    rd_renderer_text(r, n.value, RD_THEME_FUNCTION, RD_THEME_BACKGROUND);
    rd_renderer_text(r, "()", RD_THEME_FUNCTION, RD_THEME_BACKGROUND);

    if(rd_function_is_noret(f)) {
        rd_renderer_text(r, " noreturn", RD_THEME_FUNCTION,
                         RD_THEME_BACKGROUND);
    }
}

static void _rd_render_instruction_item(RDRenderer* r, const RDSegmentFull* seg,
                                        usize idx, usize sub_line) {
    RDAddress address = rd_i_renderer_new_row(r, seg, idx, sub_line, 8);

    switch(r->mode) {
        case RD_RM_RDIL: rd_i_renderer_rdil(r, address); break;
        case RD_RM_FLAGS: rd_i_renderer_flags(r, address); break;
        default: rd_i_renderer_instr(r, address); break;
    }

    _rd_render_refs(r, address);
    _rd_render_comment(r, address);
}

static void _rd_render_data_row(RDRenderer* r, const RDSegmentFull* seg,
                                usize idx, usize sub_line, bool is_banner,
                                const RDResolveResult* res) {
    // indentation is a schema property (depth), not a row ordinal: the
    // same field indents identically whether reached from its root's
    // address or its own (first.x under the root, first.y at its own
    // head - both depth 1, both at the same column). The banner sits
    // one level above depth 0
    usize indent = is_banner ? 8 : 8 + ((res->depth + 1) * 2);
    RDAddress address = rd_i_renderer_new_row(r, seg, idx, sub_line, indent);

    if(r->mode == RD_RM_FLAGS) {
        rd_i_renderer_flags(r, address);
        return;
    }

    _rd_render_modifiers(r, address, RD_THEME_TYPE, RD_THEME_BACKGROUND);

    RDType t = res->field.type;
    const char* name = res->field.name;
    const RDTypeDef* tdef = t.def;
    assert(tdef);

    // an unnamed result that crossed an array IS the element: show [n].
    // Named members never show the index - the index belongs to the
    // element's own row, wherever that renders
    bool is_element = res->item_idx.has_value && !name;
    bool skip_type_name = is_element && tdef->kind == RD_TKIND_PRIM;

    // 1. struct/union keyword for compound heads
    if(t.mod == RD_TYPE_NONE && t.count == 0) {
        if(tdef->kind == RD_TKIND_STRUCT)
            rd_renderer_text(r, "struct ", RD_THEME_TYPE, RD_THEME_BACKGROUND);
        else if(tdef->kind == RD_TKIND_UNION)
            rd_renderer_text(r, "union ", RD_THEME_TYPE, RD_THEME_BACKGROUND);
    }

    if(!skip_type_name) {
        // 2. type name
        rd_renderer_text(r, tdef->name, RD_THEME_TYPE, RD_THEME_BACKGROUND);

        // 3. pointer modifier
        if(rd_type_is_ptr(&res->field.type)) rd_renderer_norm(r, "*");
        rd_renderer_ws(r, 1);
    }

    // 4. member name or address name
    if(is_banner) {
        RDName n;
        bool hasname = rd_i_get_name(r->context, address, true, &n);
        assert(hasname && "cannot get type name");
        rd_renderer_norm(r, n.value);
    }
    else {
        if(is_element) {
            rd_renderer_norm(r, "[");
            rd_renderer_text(r, rd_i_to_dec((i64)res->item_idx.value),
                             RD_THEME_NUMBER, RD_THEME_BACKGROUND);
            rd_renderer_norm(r, "]");
        }

        if(name) rd_renderer_norm(r, name);
    }

    // 5. array size
    if(t.count > 0) {
        rd_renderer_norm(r, "[");
        rd_renderer_text(r, rd_i_to_dec((i64)t.count), RD_THEME_NUMBER,
                         RD_THEME_BACKGROUND);
        rd_renderer_norm(r, "]");
    }

    // 6. value (only for primitives and pointers, not compound heads)
    if(tdef->kind == RD_TKIND_PRIM || rd_type_is_ptr(&res->field.type)) {
        rd_renderer_ws(r, 1);
        rd_renderer_norm(r, "=");
        rd_renderer_ws(r, 1);
        _rd_render_value(r, address, &t, true);
    }

    _rd_render_comment(r, address);
}

static RDRenderItemResult _rd_render_item_unknown(RDRenderer* r,
                                                  const RDSegmentFull* seg,
                                                  usize idx, usize sub_line) {
    usize slot = 0, curridx = idx;

    if(rd_i_flagsbuffer_has_info(seg->flags, idx)) {
        if(sub_line == slot) {
            _rd_render_label_item(r, seg, idx, sub_line);
            return (RDRenderItemResult){.status = RD_ROW_OK};
        }
        slot++;
    }

    if(sub_line == slot) {
        rd_i_renderer_new_row(r, seg, idx, sub_line, 8);

        while(curridx < rd_flagsbuffer_get_length(seg->flags)) {
            if((curridx - idx) == RD_SURFACE_HEX_LINE) break;
            if(!rd_flagsbuffer_has_unknown(seg->flags, curridx)) break;

            u8 v;
            if(rd_flagsbuffer_get_value(seg->flags, curridx, &v))
                rd_renderer_norm(r, rd_i_to_hex(v, sizeof(u8)));
            else
                rd_renderer_muted(r, "??");

            rd_renderer_ws(r, 1);
            curridx++;
        }

        usize n = curridx - idx;

        if(n < RD_SURFACE_HEX_LINE)
            rd_renderer_ws(r, (RD_SURFACE_HEX_LINE - n) * 3);

        curridx = idx;

        for(usize i = 0; i < n; i++, curridx++) {
            char ptr[2] = {0};
            if(rd_flagsbuffer_get_value(seg->flags, curridx, (u8*)&ptr))
                rd_renderer_norm(r, isprint(*ptr) ? ptr : ".");
            else
                rd_renderer_muted(r, "?");
        }

        if(n < RD_SURFACE_HEX_LINE) rd_renderer_ws(r, RD_SURFACE_HEX_LINE - n);
        return (RDRenderItemResult){
            .status = RD_ROW_OK,
            .length = curridx - idx,
        };
    }

    return (RDRenderItemResult){.status = RD_ROW_EXHAUSTED};
}

static RDRenderItemResult _rd_render_item_data(RDRenderer* r,
                                               const RDSegmentFull* seg,
                                               usize idx, usize sub_line) {
    RDDataHead head;
    rd_i_data_head_get(r->context, seg, idx, &head);

    RDResolveResult res = {0};
    bool is_head = head.has_banner && sub_line == 0;
    bool ok;

    if(is_head) {
        res.field = (RDParam){.type = head.root, .name = NULL};
        ok = true;
    }
    else if(head.has_banner && !rd_i_type_has_more(&head.root)) {
        ok = false; // solid root: the banner was the whole rendering
    }
    else {
        usize link = head.has_banner ? sub_line - 1 : sub_line;
        ok = rd_i_data_chain_row(r->context, &head, link, &res);
    }

    if(!ok) return (RDRenderItemResult){.status = RD_ROW_EXHAUSTED};

    // string-like types (aka char[n], char16[n] ...) are not expanded
    // but rendered as a single line + terminator
    if(!is_head && sub_line > 0 && rd_type_is_string(&head.root))
        return (RDRenderItemResult){.status = RD_ROW_EXHAUSTED};

    _rd_render_data_row(r, seg, idx, sub_line, is_head, &res);

    // the deepest chain entity is the narrowest:
    // on EXHAUSTED the fill loop advances by the LAST OK row's length, which is
    // exactly the step from this head to the next one
    return (RDRenderItemResult){
        .status = RD_ROW_OK,
        .length = rd_type_size(&res.field.type, r->context),
    };
}

static RDRenderItemResult _rd_render_item_code(RDRenderer* r,
                                               const RDSegmentFull* seg,
                                               usize idx, usize sub_line) {
    usize slot = 0;

    if(rd_flagsbuffer_has_func(seg->flags, idx)) {
        if(sub_line == slot) {
            _rd_render_function_item(r, seg, idx, sub_line);

            return (RDRenderItemResult){
                .status = RD_ROW_OK,
                .length = rd_i_flagsbuffer_get_range_length(seg->flags, idx),
            };
        }
        slot++;
    }
    else if(rd_i_flagsbuffer_has_xref_in(seg->flags, idx)) {
        if(sub_line == slot) {
            _rd_render_label_item(r, seg, idx, sub_line);
            return (RDRenderItemResult){
                .status = RD_ROW_OK,
                .length = rd_i_flagsbuffer_get_range_length(seg->flags, idx),
            };
        }
        slot++;
    }

    if(sub_line == slot) {
        _rd_render_instruction_item(r, seg, idx, sub_line);

        return (RDRenderItemResult){
            .status = RD_ROW_OK,
            .length = rd_i_flagsbuffer_get_range_length(seg->flags, idx),
        };
    }

    slot++;

    if(rd_flagsbuffer_has_noret(seg->flags, idx)) {
        if(sub_line == slot) {
            _rd_render_comment_item(r, seg, idx, sub_line, "does not return");

            return (RDRenderItemResult){
                .status = RD_ROW_OK,
                .length = rd_i_flagsbuffer_get_range_length(seg->flags, idx),
            };
        }
        slot++;
    }

    return (RDRenderItemResult){.status = RD_ROW_EXHAUSTED};
}

bool rd_i_render_segment_item(RDRenderer* r, const RDSegmentFull* seg) {
    if(rd_i_renderer_has_flag(r, RD_RF_NO_SEGMENT)) return false;

    const RDProcessorPlugin* p = r->context->processorplugin;
    rd_i_renderer_new_row(r, seg, 0, RD_SUB_LINE_NONE, 0);

    if(p->render_segment) {
        p->render_segment(r, (const RDSegment*)seg, r->context->processor);
    }
    else {
        const unsigned int INT_SIZE = r->context->processorplugin->int_size;
        const unsigned int B = seg->base.unit ? seg->base.unit : INT_SIZE;
        const unsigned int F = B * 2;

        rd_renderer_text(r, "segment ", RD_THEME_SEGMENT, RD_THEME_BACKGROUND);
        rd_renderer_text(r, seg->base.name, RD_THEME_SEGMENT,
                         RD_THEME_BACKGROUND);
        rd_renderer_text(r, " (start: ", RD_THEME_SEGMENT, RD_THEME_BACKGROUND);
        rd_renderer_num(r, (i64)seg->base.start_address, 16, F, RD_NUM_NOADDR);
        rd_renderer_text(r, ", end: ", RD_THEME_SEGMENT, RD_THEME_BACKGROUND);
        rd_renderer_num(r, (i64)seg->base.end_address, 16, F, RD_NUM_NOADDR);
        rd_renderer_text(r, ")", RD_THEME_SEGMENT, RD_THEME_BACKGROUND);
    }

    return true;
}

void rd_i_data_head_get(RDContext* ctx, const RDSegmentFull* seg, usize idx,
                        RDDataHead* out) {
    RDAddress address = seg->base.start_address + idx;

    if(rd_flagsbuffer_has_type(seg->flags, idx)) {
        RDTypeFull t;
        bool got = rd_i_db_get_type(ctx, address, &t);
        panic_if(!got, "type not found @ %s:%x", seg->base.name, address);

        *out = (RDDataHead){.root = t.base, .offset = 0, .has_banner = true};
        return;
    }

    if(rd_flagsbuffer_has_field(seg->flags, idx) ||
       rd_flagsbuffer_has_item(seg->flags, idx)) {
        RDAddress root_address = address;
        RDType root;
        bool got = rd_i_db_get_root_type(ctx, &root_address, &root);
        panic_if(!got, "root type not found @ %s:%x", seg->base.name, address);

        *out = (RDDataHead){.root = root, .offset = address - root_address};
        return;
    }

    unreachable();
}

bool rd_i_data_chain_row(RDContext* ctx, const RDDataHead* head, usize link,
                         RDResolveResult* out) {
    RDResolveResult probe;
    if(!rd_type_resolve_offset(ctx, &head->root, head->offset, 0, &probe))
        return false;

    if(link == 0) {
        *out = probe;
        return true;
    }

    usize want = probe.depth + link;
    if(!rd_type_resolve_offset(ctx, &head->root, head->offset, want, out))
        return false;

    return out->depth == want; // shallower: the chain ended before this link
}

RDRenderItemResult rd_i_render_item(RDRenderer* r, const RDSegmentFull* seg,
                                    usize idx, usize sub_line) {
    panic_if(rd_flagsbuffer_has_tail(seg->flags, idx),
             "tail detected @ %" PRIx64 ", sub_line %zu",
             seg->base.start_address + idx, sub_line);

    if(rd_flagsbuffer_has_unknown(seg->flags, idx))
        return _rd_render_item_unknown(r, seg, idx, sub_line);

    if(rd_flagsbuffer_has_data(seg->flags, idx))
        return _rd_render_item_data(r, seg, idx, sub_line);

    if(rd_flagsbuffer_has_code(seg->flags, idx))
        return _rd_render_item_code(r, seg, idx, sub_line);

    unreachable();
}

RDRenderItemResult rd_i_render_item_any(RDRenderer* r, const RDSegmentFull* seg,
                                        usize idx) {
    usize sub_line = 0;

    if(rd_flagsbuffer_has_code(seg->flags, idx))
        sub_line = rd_i_row_code_instr_sub_line(seg, idx);

    return rd_i_render_item(r, seg, idx, sub_line);
}
