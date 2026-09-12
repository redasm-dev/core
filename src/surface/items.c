#include "items.h"
#include "core/context.h"
#include "io/flagsbuffer.h"
#include "support/containers.h"
#include "support/error.h"
#include "surface/layout.h"
#include <ctype.h>
#include <inttypes.h>

#define RD_SURFACE_WS_COMMENT 8
#define RD_SURFACE_WS_REFS 8

static void _rd_render_modifiers(RDRenderer* r, const RDSegmentFull* seg,
                                 usize idx, RDThemeKind fg, RDThemeKind bg) {
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
    else if(t->mod == RD_TYPE_CPTR || t->def->kind == RD_TKIND_FUNC) {
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

    u64 v; // numeric primitive
    if(rd_i_buffer_read_primitive(flags, idx, t->def->name, is_be, &v)) {
        unsigned int sz =
            (unsigned int)rd_i_size_of(r->context, t->def->name, 0, t->mod);
        panic_if(!sz, "type '%s' has unresolved size", t->def->name);

        if(sz == PTR_SIZE && rd_i_flagsbuffer_has_xref_in(seg->flags, idx))
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

static void _rd_render_comment_inline(RDRenderer* r, RDAddress address) {
    if(rd_i_renderer_has_flag(r, RD_RF_NO_COMMENTS)) return;

    const char* cmt = rd_get_comment_inline(r->context, address);
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

static void _rd_render_segment_row(RDRenderer* r, const RDSegmentFull* seg,
                                   usize idx, usize sub_line, usize indent) {
    const RDProcessorPlugin* p = r->context->processorplugin;
    rd_i_renderer_new_row(r, seg, idx, sub_line, indent);

    if(p->render_segment) {
        p->render_segment(r, (const RDSegment*)seg, r->context->processor);
        return;
    }

    const unsigned int INT_SIZE = rd_get_ptr_size(r->context);
    const unsigned int F = INT_SIZE * 2;

    rd_renderer_text(r, "segment ", RD_THEME_SEGMENT, RD_THEME_BACKGROUND);
    rd_renderer_text(r, seg->base.name, RD_THEME_SEGMENT, RD_THEME_BACKGROUND);
    rd_renderer_text(r, " (start: ", RD_THEME_SEGMENT, RD_THEME_BACKGROUND);
    rd_renderer_num(r, (i64)seg->base.start_address, 16, F, RD_NUM_NOADDR);
    rd_renderer_text(r, ", end: ", RD_THEME_SEGMENT, RD_THEME_BACKGROUND);
    rd_renderer_num(r, (i64)seg->base.end_address, 16, F, RD_NUM_NOADDR);
    rd_renderer_text(r, ")", RD_THEME_SEGMENT, RD_THEME_BACKGROUND);
}

static void _rd_render_comment_row(RDRenderer* r, const RDSegmentFull* seg,
                                   usize idx, usize sub_line,
                                   const char* comment, usize indent) {
    rd_i_renderer_new_row(r, seg, idx, sub_line, indent);
    if(!comment || !(*comment)) return; // just an empty line

    rd_renderer_text(r, "; ", RD_THEME_MUTED, RD_THEME_BACKGROUND);
    rd_renderer_text(r, comment, RD_THEME_MUTED, RD_THEME_BACKGROUND);
}

static void _rd_render_label_row(RDRenderer* r, const RDSegmentFull* seg,
                                 usize idx, usize sub_line, usize indent) {
    RDAddress address = rd_i_renderer_new_row(r, seg, idx, sub_line, indent);

    RDName n;
    bool hasname = rd_i_get_name(r->context, address, true, &n);
    assert(hasname && "cannot get label name");

    rd_renderer_text(r, n.value, RD_THEME_LOCATION, RD_THEME_BACKGROUND);
    rd_renderer_text(r, ":", RD_THEME_LOCATION, RD_THEME_BACKGROUND);
}

static void _rd_render_function_row(RDRenderer* r, const RDSegmentFull* seg,
                                    usize idx, usize sub_line, usize indent) {
    const RDProcessorPlugin* p = r->context->processorplugin;
    RDAddress address = rd_i_renderer_new_row(r, seg, idx, sub_line, indent);

    // RDFunction may be NULL (eg. during intermediate state analysis)
    const RDFunction* f = rd_i_find_function(r->context, address);

    if(f && p->render_function) {
        p->render_function(r, f, r->context->processor);
        return;
    }

    _rd_render_modifiers(r, seg, idx, RD_THEME_FUNCTION, RD_THEME_BACKGROUND);

    if(f) {
        const RDFunctionType* f_type = &f->type_def->func_;

        if(f_type->ret.has_value) {
            rd_renderer_text(r,
                             rd_i_type_to_str(&f_type->ret.value, &r->type_buf),
                             RD_THEME_FUNCTION, RD_THEME_BACKGROUND);
            rd_renderer_ws(r, 1);
        }

        if(rd_function_is_noret(f)) {
            rd_renderer_text(r, "noreturn ", RD_THEME_FUNCTION,
                             RD_THEME_BACKGROUND);
        }
    }

    if(!f || (f->type_def->flags & RD_TFLAGS_BUILTIN)) {
        rd_renderer_text(r, "function ", RD_THEME_FUNCTION,
                         RD_THEME_BACKGROUND);
    }

    RDName n;
    bool hasname = rd_i_get_name(r->context, address, true, &n);
    assert(hasname);
    rd_renderer_text(r, n.value, RD_THEME_FUNCTION, RD_THEME_BACKGROUND);

    if(f) {
        const RDFunctionType* f_type = &f->type_def->func_;

        if(f_type->args.has_value) {
            rd_renderer_text(r, "(", RD_THEME_FUNCTION, RD_THEME_BACKGROUND);

            const RDParam* arg;
            vect_each(arg, &f_type->args.value) {
                assert(arg->name);

                if(arg != vect_first(&f_type->args.value)) {
                    rd_renderer_text(r, ",", RD_THEME_FUNCTION,
                                     RD_THEME_BACKGROUND);
                }

                rd_renderer_text(r, rd_i_type_to_str(&arg->type, &r->type_buf),
                                 RD_THEME_FUNCTION, RD_THEME_BACKGROUND);
                rd_renderer_ws(r, 1);
                rd_renderer_text(r, arg->name, RD_THEME_FUNCTION,
                                 RD_THEME_BACKGROUND);
            }

            rd_renderer_text(r, ")", RD_THEME_FUNCTION, RD_THEME_BACKGROUND);
        }
    }
}

static void _rd_render_elements_row(RDRenderer* r, const RDSegmentFull* seg,
                                    usize idx, usize sub_line,
                                    const RDRowDesc* d) {
    RDAddress address = rd_i_renderer_new_row(r, seg, idx, sub_line, d->indent);

    if(r->mode == RD_RM_FLAGS) {
        rd_i_renderer_flags(r, address);
        return;
    }

    usize sz = rd_type_size(&d->elements.type, r->context);

    usize lead = (address % RD_SURFACE_HEX_LINE) / sz;
    if(lead) rd_renderer_ws(r, lead * ((sz * 2) + 1));

    for(usize i = 0; i < d->elements.count; i++) {
        if(i) rd_renderer_ws(r, 1);
        _rd_render_value(r, address + (i * sz), &d->elements.type, false);
    }

    _rd_render_comment_inline(r, address);
}

static void _rd_render_instruction_row(RDRenderer* r, const RDSegmentFull* seg,
                                       usize idx, usize sub_line,
                                       usize indent) {
    RDAddress address = rd_i_renderer_new_row(r, seg, idx, sub_line, indent);

    switch(r->mode) {
        case RD_RM_RDIL: rd_i_renderer_rdil(r, address); break;
        case RD_RM_FLAGS: rd_i_renderer_flags(r, address); break;
        default: rd_i_renderer_instr(r, address); break;
    }

    _rd_render_refs(r, address);
    _rd_render_comment_inline(r, address);
}

static void _rd_render_hexdump_row(RDRenderer* r, const RDSegmentFull* seg,
                                   usize idx, usize sub_line, usize len,
                                   usize indent) {
    rd_i_renderer_new_row(r, seg, idx, sub_line, indent);

    usize lead = (seg->base.start_address + idx) % RD_SURFACE_HEX_LINE;
    if(lead) rd_renderer_ws(r, lead * 3); // hex column: 3 chars per byte

    for(usize i = 0; i < len; i++) {
        u8 v;

        if(rd_flagsbuffer_get_value(seg->flags, idx + i, &v))
            rd_renderer_norm(r, rd_i_to_hex(v, sizeof(u8)));
        else
            rd_renderer_muted(r, "??");

        rd_renderer_ws(r, 1);
    }

    if(lead + len < RD_SURFACE_HEX_LINE)
        rd_renderer_ws(r, (RD_SURFACE_HEX_LINE - lead - len) * 3);

    if(lead) rd_renderer_ws(r, lead); // ascii column: 1 char per byte

    for(usize i = 0; i < len; i++) {
        char ptr[2] = {0};

        if(rd_flagsbuffer_get_value(seg->flags, idx + i, (u8*)&ptr))
            rd_renderer_norm(r, isprint(*ptr) ? ptr : ".");
        else
            rd_renderer_muted(r, "?");
    }

    if(lead + len < RD_SURFACE_HEX_LINE)
        rd_renderer_ws(r, RD_SURFACE_HEX_LINE - lead - len);
}

static void _rd_render_data_row(RDRenderer* r, const RDSegmentFull* seg,
                                usize idx, usize sub_line, const RDRowDesc* d) {
    bool is_banner = d->kind == RD_ROWKIND_DATA_BANNER;
    RDAddress address = rd_i_renderer_new_row(r, seg, idx, sub_line, d->indent);

    if(r->mode == RD_RM_FLAGS) {
        rd_i_renderer_flags(r, address);
        return;
    }

    if(d->is_head_row)
        _rd_render_modifiers(r, seg, idx, RD_THEME_TYPE, RD_THEME_BACKGROUND);

    RDType t = d->resolve.field.type;
    const char* name = d->resolve.field.name;
    const RDTypeDef* tdef = t.def;
    assert(tdef);

    bool is_element = d->resolve.item_idx.has_value && !name;
    bool skip_type_name = (is_element && tdef->kind == RD_TKIND_PRIM) ||
                          tdef->kind == RD_TKIND_FUNC;

    // 1a. struct/union keyword for compound heads
    if(t.mod == RD_TYPE_NONE && t.count == 0) {
        if(tdef->kind == RD_TKIND_STRUCT)
            rd_renderer_text(r, "struct ", RD_THEME_TYPE, RD_THEME_BACKGROUND);
        else if(tdef->kind == RD_TKIND_UNION)
            rd_renderer_text(r, "union ", RD_THEME_TYPE, RD_THEME_BACKGROUND);
    }

    // 1b. function keyword
    if(tdef->kind == RD_TKIND_FUNC) {
        if(tdef->func_.is_noret) {
            rd_renderer_text(r, "noreturn ", RD_THEME_TYPE,
                             RD_THEME_BACKGROUND);
        }

        rd_renderer_text(r, "function ", RD_THEME_TYPE, RD_THEME_BACKGROUND);
    }

    if(!skip_type_name) {
        // 2. type name
        rd_renderer_text(r, tdef->name, RD_THEME_TYPE, RD_THEME_BACKGROUND);

        // 3. pointer modifier
        if(rd_type_is_ptr(&d->resolve.field.type)) rd_renderer_norm(r, "*");
        rd_renderer_ws(r, 1);
    }

    // 4. member name or address name
    if(is_banner) {
        RDName n;
        bool hasname = rd_i_get_name(r->context, address, true, &n);
        assert(hasname && "cannot get type name");
        if(hasname) rd_renderer_norm(r, n.value);
    }
    else if(is_element) {
        RDName n;

        if(rd_i_typedef_is_compound(tdef) &&
           rd_i_db_get_name(r->context, address, &n))
            rd_renderer_norm(r, n.value);
        else {
            rd_renderer_norm(r, "[");
            rd_renderer_text(r, rd_i_to_dec((i64)d->resolve.item_idx.value),
                             RD_THEME_NUMBER, RD_THEME_BACKGROUND);
            rd_renderer_norm(r, "]");
        }
    }
    else if(name)
        rd_renderer_norm(r, name);

    // 5. array size
    if(t.count > 0) {
        rd_renderer_norm(r, "[");
        rd_renderer_text(r, rd_i_to_dec((i64)t.count), RD_THEME_NUMBER,
                         RD_THEME_BACKGROUND);
        rd_renderer_norm(r, "]");
    }

    // 6. value
    bool has_children = rd_i_type_has_more(&d->resolve.field.type);

    bool has_value = !has_children && (tdef->kind == RD_TKIND_PRIM ||
                                       tdef->kind == RD_TKIND_FUNC ||
                                       rd_type_is_ptr(&d->resolve.field.type));

    if(has_children || has_value) {
        rd_renderer_ws(r, 1);
        rd_renderer_norm(r, "=");
    }

    if(has_value) {
        rd_renderer_ws(r, 1);
        _rd_render_value(r, address, &t, true);
    }

    _rd_render_comment_inline(r, address);
}

void rd_i_render_row(RDRenderer* r, const RDSegmentFull* seg, usize idx,
                     usize sub_line, const RDRowDesc* d) {
    usize before = vect_length(&r->rows_back);

    switch(d->kind) {
        case RD_ROWKIND_SEGMENT:
            _rd_render_segment_row(r, seg, idx, sub_line, d->indent);
            break;

        case RD_ROWKIND_COMMENT_BEFORE:
        case RD_ROWKIND_COMMENT_AFTER: {
            RDAddress address = seg->base.start_address + idx;

            RDCommentPlacement p = (d->kind == RD_ROWKIND_COMMENT_BEFORE)
                                       ? RD_COMMENT_BEFORE
                                       : RD_COMMENT_AFTER;

            const char* text =
                rd_i_db_get_comment(r->context, address, p, d->comment_idx);

            _rd_render_comment_row(r, seg, idx, sub_line, text, d->indent);
            break;
        }

        case RD_ROWKIND_FUNCTION:
            _rd_render_function_row(r, seg, idx, sub_line, d->indent);
            break;

        case RD_ROWKIND_LABEL:
            _rd_render_label_row(r, seg, idx, sub_line, d->indent);
            break;

        case RD_ROWKIND_INSTRUCTION:
            _rd_render_instruction_row(r, seg, idx, sub_line, d->indent);
            break;

        case RD_ROWKIND_NORET: {
            _rd_render_comment_row(r, seg, idx, sub_line, "does not return",
                                   d->indent);
            break;
        }

        case RD_ROWKIND_HEXDUMP:
            _rd_render_hexdump_row(r, seg, idx, sub_line, d->length, d->indent);
            break;

        case RD_ROWKIND_DATA_BANNER:
        case RD_ROWKIND_DATA_LINK: {
            _rd_render_data_row(r, seg, idx, sub_line, d);
            break;
        }

        case RD_ROWKIND_DATA_ELEMENTS:
            _rd_render_elements_row(r, seg, idx, sub_line, d);
            break;

        default: unreachable();
    }

    panic_if(vect_length(&r->rows_back) != before + 1,
             "row body for kind %d emitted %zu rows, expected 1", d->kind,
             vect_length(&r->rows_back) - before);

    vect_last(&r->rows_back)->kind = d->kind;
}

void rd_i_render_item(RDRenderer* r, const RDSegmentFull* seg, usize idx,
                      usize sub_line) {
    rd_i_item_layout(r->context, r->flags, seg, idx, &r->layout_buf);
    if(sub_line >= vect_length(&r->layout_buf)) return;

    rd_i_render_row(r, seg, idx, sub_line, vect_at(&r->layout_buf, sub_line));
}

void rd_i_render_item_any(RDRenderer* r, const RDSegmentFull* seg, usize idx) {
    rd_i_item_layout(r->context, r->flags, seg, idx, &r->layout_buf);

    usize sub_line = rd_i_item_layout_content(&r->layout_buf);
    rd_i_render_row(r, seg, idx, sub_line, vect_at(&r->layout_buf, sub_line));
}
