#include "items.h"
#include "core/context.h"
#include "support/containers.h"
#include "support/error.h"
#include <ctype.h>

#define RD_SURFACE_WS_COMMENT 8
#define RD_SURFACE_WS_REFS 4

static void _rd_render_modifiers(RDRenderer* r, const RDListingItem* item,
                                 RDThemeKind fg, RDThemeKind bg) {
    if(item->mod == RD_LMOD_IMPORTED)
        rd_renderer_text(r, "imported ", fg, bg);
    else if(item->mod == RD_LMOD_EXPORTED)
        rd_renderer_text(r, "exported ", fg, bg);
}

static void _rd_render_value(RDRenderer* r, RDAddress address, const RDType* t,
                             bool term) {
    const RDSegmentFull* seg = rd_i_find_segment(r->context, address);
    panic_if(!seg, "_rd_surface_render_value: invalid segment");

    const RDBuffer* flags = (const RDBuffer*)seg->flags;
    const unsigned int PTR_SIZE = rd_processor_get_ptr_size(r->context);
    bool is_be = rd_processor_get_flags(r->context) & RD_PF_BE;
    usize idx = rd_i_address2index(seg, address);

    // pointer
    if(t->flags & RD_TYPE_ISPOINTER) {
        const char* ptrtype = rd_integral_from_size(PTR_SIZE);
        u64 v;

        if(rd_i_buffer_read_primitive(flags, idx, ptrtype, is_be, &v)) {
            const unsigned int F = PTR_SIZE * 2;
            rd_renderer_loc(r, v, F, RD_NUM_DEFAULT);
        }
        else
            rd_renderer_nop(r, "?");

        return;
    }

    // char array - render as string
    if(!strcmp(t->name, "char") && t->count > 0) {
        rd_renderer_text(r, "\"", RD_THEME_STRING, RD_THEME_BACKGROUND);
        for(usize i = 0; i < t->count; i++) {
            u8 v;

            if(!rd_i_buffer_read_u8(flags, idx + i, &v)) {
                rd_renderer_nop(r, "?");
                continue;
            }

            if(!v) break;

            rd_renderer_text(r, rd_i_escape_char(v, true), RD_THEME_STRING,
                             RD_THEME_BACKGROUND);
        }
        rd_renderer_text(r, "\"", RD_THEME_STRING, RD_THEME_BACKGROUND);
        if(term) {
            rd_renderer_norm(r, ",");
            rd_renderer_cnst(r, 0, 10, 0, RD_NUM_DEFAULT);
        }
        return;
    }

    // char16 array - render as string
    if(!strcmp(t->name, "char16") && t->count > 0) {
        rd_renderer_text(r, "\"", RD_THEME_STRING, RD_THEME_BACKGROUND);
        for(usize i = 0; i < t->count; i++) {
            u16 v;
            bool ok = is_be ? rd_i_buffer_read_be16(flags, idx, &v)
                            : rd_i_buffer_read_le16(flags, idx, &v);

            if(!ok) {
                rd_renderer_nop(r, "?");
                continue;
            }

            if(!v) break;

            rd_renderer_text(r, rd_i_escape_char16(v, true), RD_THEME_STRING,
                             RD_THEME_BACKGROUND);
        }
        rd_renderer_text(r, "\"", RD_THEME_STRING, RD_THEME_BACKGROUND);
        if(term) {
            rd_renderer_norm(r, ",");
            rd_renderer_cnst(r, 0, 10, 0, RD_NUM_DEFAULT);
        }
        return;
    }

    // single char
    if(!strcmp(t->name, "char")) {
        u8 v;
        rd_renderer_norm(r, "'");
        if(rd_i_buffer_read_u8(flags, idx, &v))
            rd_renderer_text(r, rd_i_escape_char(v, false), RD_THEME_STRING,
                             RD_THEME_BACKGROUND);
        else
            rd_renderer_nop(r, "?");
        rd_renderer_norm(r, "'");
        return;
    }

    // single char16
    if(!strcmp(t->name, "char16")) {
        u16 v;
        rd_renderer_norm(r, "'");

        bool ok = is_be ? rd_i_buffer_read_be16(flags, idx, &v)
                        : rd_i_buffer_read_le16(flags, idx, &v);

        if(ok)
            rd_renderer_text(r, rd_i_escape_char16(v, false), RD_THEME_STRING,
                             RD_THEME_BACKGROUND);
        else
            rd_renderer_nop(r, "?");
        rd_renderer_norm(r, "'");
        return;
    }

    // numeric primitive
    u64 v;
    if(rd_i_buffer_read_primitive(flags, idx, t->name, is_be, &v)) {
        usize sz = rd_i_size_of(r->context, t->name, 0, t->flags);
        rd_renderer_cnst(r, v, 16, sz * 2, RD_NUM_DEFAULT);
    }
    else
        rd_renderer_nop(r, "?");
}

static void _rd_render_refs(RDRenderer* r, const RDListingItem* item) {
    if(rd_i_renderer_has_flag(r, RD_RF_NO_REFS)) return;

    RDContext* ctx = r->context;
    if(!rd_i_get_xrefs_from_ex(ctx, item->address, &r->xrefs)) return;

    const RDXRef* xref;
    vect_each(xref, &r->xrefs) {
        const RDSegmentFull* seg = rd_i_find_segment(ctx, xref->address);
        if(!seg) continue;

        RDTypeFull t; // render strings only
        if(!rd_i_get_type(ctx, xref->address, &t)) continue;
        if(strcmp(t.base.name, "char") != 0 || !t.base.count) continue;

        rd_renderer_ws(r, RD_SURFACE_WS_REFS);
        _rd_render_value(r, xref->address, &t.base, false);
    }
}

static void _rd_render_comment(RDRenderer* r, const RDListingItem* item) {
    if(rd_i_renderer_has_flag(r, RD_RF_NO_COMMENTS)) return;

    const char* cmt = rd_get_comment(r->context, item->address);
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

static void _rd_render_hex_dump_item(RDRenderer* r, const RDListingItem* item) {
    const RDSegmentFull* seg = rd_i_find_segment(r->context, item->address);
    panic_if(!seg, "_rd_surface_render_hex_dump: invalid segment");

    int c = 0;
    rd_i_renderer_new_row(r, item);

    for(RDAddress addr = item->start_address; addr < item->end_address;
        addr++, c += 3) {
        u8 v;
        usize idx = rd_i_address2index(seg, addr);

        if(rd_flagsbuffer_get_value(seg->flags, idx, &v))
            rd_renderer_norm(r, rd_i_to_hex(v, sizeof(u8)));
        else
            rd_renderer_nop(r, "??");

        rd_renderer_ws(r, 1);
    }

    if(c < RD_LISTING_HEX_LINE * 3)
        rd_renderer_ws(r, (RD_LISTING_HEX_LINE * 3) - c);

    c = 0;

    for(RDAddress addr = item->start_address; addr < item->end_address;
        addr++, c++) {
        char ptr[2] = {0};
        usize idx = rd_i_address2index(seg, addr);

        if(rd_flagsbuffer_get_value(seg->flags, idx, (u8*)&ptr))
            rd_renderer_norm(r, isprint(*ptr) ? ptr : ".");
        else
            rd_renderer_nop(r, "?");
    }

    if(c < RD_LISTING_HEX_LINE) rd_renderer_ws(r, RD_LISTING_HEX_LINE - c);
}

static void _rd_render_fill_item(RDRenderer* r, const RDListingItem* item) {
    usize count = item->end_address - item->start_address;

    rd_i_renderer_new_row(r, item);
    rd_renderer_text(r, ".fill ", RD_THEME_FUNCTION, RD_THEME_BACKGROUND);
    rd_renderer_cnst(r, count, 16, 0, RD_NUM_DEFAULT);
    rd_renderer_norm(r, ",");

    if(item->fill.has_value)
        rd_renderer_cnst(r, item->fill.value, 16, 2, RD_NUM_DEFAULT);
    else
        rd_renderer_nop(r, "??");
}

static void _rd_render_segment_item(RDRenderer* r, const RDListingItem* item) {
    if(rd_i_renderer_has_flag(r, RD_RF_NO_SEGMENT)) return;

    const RDSegmentFull* seg = item->segment;
    rd_i_renderer_new_row(r, item);

    if(rd_i_processor_has_render_segment(r->context)) {
        rd_i_processor_render_segment(r->context, r, (const RDSegment*)seg);
    }
    else {
        const unsigned int PROC_INT = rd_processor_get_int_size(r->context);
        const unsigned int B = seg->base.unit ? seg->base.unit : PROC_INT;
        const unsigned int F = B * 2;

        rd_renderer_text(r, "segment ", RD_THEME_SEGMENT, RD_THEME_BACKGROUND);
        rd_renderer_text(r, seg->base.name, RD_THEME_SEGMENT,
                         RD_THEME_BACKGROUND);
        rd_renderer_text(r, " (start: ", RD_THEME_SEGMENT, RD_THEME_BACKGROUND);
        rd_renderer_cnst(r, seg->base.start_address, 16, F, RD_NUM_DEFAULT);
        rd_renderer_text(r, ", end: ", RD_THEME_SEGMENT, RD_THEME_BACKGROUND);
        rd_renderer_cnst(r, seg->base.end_address, 16, F, RD_NUM_DEFAULT);
        rd_renderer_text(r, ")", RD_THEME_SEGMENT, RD_THEME_BACKGROUND);
    }
}

static void _rd_render_function_item(RDRenderer* r, const RDListingItem* item) {
    if(rd_i_renderer_has_flag(r, RD_RF_NO_FUNCTION)) return;

    rd_i_renderer_new_row(r, item);

    if(rd_i_processor_has_render_function(r->context)) {
        rd_i_processor_render_function(r->context, r, item->func);
    }
    else {
        RDName n;
        bool hasname = rd_i_get_name(r->context, item->address, true, &n);
        assert(hasname && "cannot get function name");

        _rd_render_modifiers(r, item, RD_THEME_FUNCTION, RD_THEME_BACKGROUND);

        rd_renderer_text(r, "function ", RD_THEME_FUNCTION,
                         RD_THEME_BACKGROUND);

        rd_renderer_text(r, n.value, RD_THEME_FUNCTION, RD_THEME_BACKGROUND);
        rd_renderer_text(r, "()", RD_THEME_FUNCTION, RD_THEME_BACKGROUND);
    }
}

static void _rd_render_label_item(RDRenderer* r, const RDListingItem* item) {
    RDName n;
    bool hasname = rd_i_get_name(r->context, item->address, true, &n);
    assert(hasname && "cannot get label name");

    rd_i_renderer_new_row(r, item);
    rd_renderer_text(r, n.value, RD_THEME_LOCATION, RD_THEME_BACKGROUND);
    rd_renderer_text(r, ":", RD_THEME_LOCATION, RD_THEME_BACKGROUND);
}

static void _rd_render_instruction_item(RDRenderer* r,
                                        const RDListingItem* item) {
    rd_i_renderer_new_row(r, item);

    switch(r->mode) {
        case RD_RM_RDIL: rd_i_renderer_rdil(r, item); break;
        case RD_RM_FLAGS: rd_i_renderer_flags(r, item); break;
        default: rd_i_renderer_instr(r, item); break;
    }

    _rd_render_refs(r, item);
    _rd_render_comment(r, item);
}

static void _rd_render_type_item(RDRenderer* r, const RDListingItem* item) {
    assert(item->type.name && "_rd_surface_render_type: invalid type");

    RDContext* ctx = r->context;
    const RDTypeDef* tdef = rd_i_typedef_find(ctx, item->type.name, true);

    rd_i_renderer_new_row(r, item);
    _rd_render_modifiers(r, item, RD_THEME_TYPE, RD_THEME_BACKGROUND);

    // 1. struct/union keyword for compound heads
    if(tdef->kind == RD_TKIND_STRUCT && !item->parent_tdef) {
        rd_renderer_text(r, "struct ", RD_THEME_TYPE, RD_THEME_BACKGROUND);
    }
    else if(tdef->kind == RD_TKIND_UNION && !item->parent_tdef) {
        rd_renderer_text(r, "union ", RD_THEME_TYPE, RD_THEME_BACKGROUND);
    }

    // 2. type name
    rd_renderer_text(r, item->type.name, RD_THEME_TYPE, RD_THEME_BACKGROUND);

    // 3. pointer modifier
    if(item->type.flags & RD_TYPE_ISPOINTER) rd_renderer_norm(r, "*");

    rd_renderer_ws(r, 1);

    // 4. member name or address name
    if(item->member_index != RD_LISTING_NO_INDEX) {
        const RDTypeDef* parent_tdef = item->parent_tdef;
        assert(parent_tdef && "member_index without parent");

        const char* member_name =
            vect_at(&parent_tdef->compound_, item->member_index)->name;

        rd_renderer_norm(r, member_name);
    }
    else {
        RDName n;
        bool hasname = rd_i_get_name(ctx, item->address, true, &n);
        assert(hasname && "cannot get type name");
        rd_renderer_norm(r, n.value);
    }

    if(item->array_index != RD_LISTING_NO_INDEX) {
        rd_renderer_norm(r, "[");
        rd_renderer_text(r, rd_i_to_dec(item->array_index), RD_THEME_NUMBER,
                         RD_THEME_BACKGROUND);
        rd_renderer_norm(r, "]");
    }

    // 5. array size
    if(item->type.count > 0) {
        rd_renderer_norm(r, "[");
        rd_renderer_text(r, rd_i_to_dec(item->type.count), RD_THEME_NUMBER,
                         RD_THEME_BACKGROUND);
        rd_renderer_norm(r, "]");
    }

    // 6. value (only for primitives and pointers, not compound heads)
    if(tdef->kind == RD_TKIND_PRIM || (item->type.flags & RD_TYPE_ISPOINTER)) {
        rd_renderer_ws(r, 1);
        rd_renderer_norm(r, "=");
        rd_renderer_ws(r, 1);
        _rd_render_value(r, item->address, &item->type, true);
    }

    _rd_render_comment(r, item);
}

void rd_i_render_item_at(RDRenderer* r, LIndex idx) {
    const RDListing* listing = &r->context->listing;
    const RDListingItem* item = vect_at(listing, idx);
    rd_i_renderer_set_current_item(r, idx, item);

    // clang-format off
    switch(item->kind) {
        case RD_LK_EMPTY:       rd_i_renderer_new_row(r, item);       break;
        case RD_LK_HEX_DUMP:    _rd_render_hex_dump_item(r, item);    break;
        case RD_LK_FILL:        _rd_render_fill_item(r, item);        break;
        case RD_LK_SEGMENT:     _rd_render_segment_item(r, item);     break;
        case RD_LK_FUNCTION:    _rd_render_function_item(r, item);    break;
        case RD_LK_LABEL:       _rd_render_label_item(r, item);       break;
        case RD_LK_INSTRUCTION: _rd_render_instruction_item(r, item); break;
        case RD_LK_TYPE:        _rd_render_type_item(r, item);        break;
        default:                unreachable();
    }
    // clang-format on
}
