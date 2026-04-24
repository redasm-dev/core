#include "builder.h"
#include "core/context.h"
#include "core/function.h"
#include "io/flagsbuffer.h"
#include "support/containers.h"
#include "support/error.h"
#include "support/logging.h"
#include <string.h>

typedef struct RDListingBuilder {
    RDContext* context;
    RDListing listing;
    const RDSegmentFull* segment;
    const RDFlagsBuffer* flags;
    RDAddress address;
} RDListingBuilder;

static usize _rd_listing_count_fill(RDListingBuilder* b, usize startidx,
                                    RDListingFillByte* fb) {
    fb->has_value = rd_flagsbuffer_get_value(b->flags, startidx, &fb->value);
    usize i;

    for(i = startidx + 1; i < b->flags->base.length; i++) {
        if(!rd_i_flagsbuffer_has_unknown(b->flags, i)) break;
        if(rd_i_flagsbuffer_has_info(b->flags, i)) break;

        u8 v;
        bool hasvalue = rd_flagsbuffer_get_value(b->flags, i, &v);
        if(fb->has_value != hasvalue) break;
        if(hasvalue && fb->value != v) break;
    }

    return i - startidx;
}

static RDListingModifier _rd_listing_check_modifiers(RDListingBuilder* b,
                                                     usize index) {
    if(rd_i_flagsbuffer_has_exported(b->flags, index)) {
        vect_push(&b->listing.exported, b->address);
        vect_push(&b->listing.symbols, (RDSymbol){
                                           .kind = RD_SYMBOL_EXPORTED,
                                           .address = b->address,
                                       });
        return RD_LMOD_EXPORTED;
    }

    if(rd_i_flagsbuffer_has_imported(b->flags, index)) {
        vect_push(&b->listing.imported, b->address);
        vect_push(&b->listing.symbols, (RDSymbol){
                                           .kind = RD_SYMBOL_IMPORTED,
                                           .address = b->address,
                                       });
        return RD_LMOD_IMPORTED;
    }

    return RD_LMOD_NONE;
}

static void _rd_listing_process_unknown(RDListingBuilder* b) {
    RDAddress startaddress = b->address;
    usize len = 0;

    while(b->address < b->segment->base.end_address) {
        usize idx = rd_i_address2index(b->segment, b->address);
        if(!rd_i_flagsbuffer_has_unknown(b->flags, idx)) break;

        // split on info flags
        if(rd_i_flagsbuffer_has_info(b->flags, idx)) {
            if(len) {
                rd_i_listing_add_hex_dump(&b->listing, startaddress,
                                          b->address);
                startaddress = b->address;
                len = 0;
            }

            _rd_listing_check_modifiers(b, idx);
            rd_i_listing_pop_indent(&b->listing, 2);
            rd_i_listing_add_label(&b->listing, b->address);
            rd_i_listing_push_indent(&b->listing, 2);
        }

        // check for fill run when starting fresh
        RDListingFillByte fb;
        usize fillcount = _rd_listing_count_fill(b, idx, &fb);

        if(fillcount > RD_LISTING_HEX_LINE) {
            if(len)
                rd_i_listing_add_hex_dump(&b->listing, startaddress,
                                          b->address);

            rd_i_listing_add_fill(&b->listing, b->address,
                                  b->address + fillcount, fb);
            b->address += fillcount;
            startaddress = b->address;
            len = 0;
            continue;
        }

        if(len && !(len % RD_LISTING_HEX_LINE)) {
            rd_i_listing_add_hex_dump(&b->listing, startaddress, b->address);
            startaddress = b->address;
            len = 0;
        }

        b->address++;
        len++;
    }

    if(b->address > startaddress)
        rd_i_listing_add_hex_dump(&b->listing, startaddress, b->address);
}

static LIndex _rd_listing_process_type(RDListingBuilder* b, const RDType* t,
                                       bool isroot) {
    RDAddress startaddr = b->address;
    LIndex idx = rd_i_listing_add_type(&b->listing, b->address, t);
    const RDTypeDef* tdef = rd_i_typedef_find(b->context, t->name, true);

    if(t->count > 0) {
        // char arrays are strings: don't expand elements
        if(!strcmp(t->name, "char") || !strcmp(t->name, "char16")) {
            if(isroot) {
                vect_push(&b->listing.symbols, (RDSymbol){
                                                   .kind = RD_SYMBOL_STRING,
                                                   .address = b->address,
                                               });
            }

            b->address += rd_i_size_of(b->context, t->name, t->count, t->flags);
            return idx;
        }

        RDType elem = *t;
        elem.count = 0; // single element's type
        rd_i_listing_push_indent(&b->listing, 1);

        for(usize i = 0; i < t->count; i++) {
            LIndex eidx = _rd_listing_process_type(b, &elem, false);
            vect_at(&b->listing, eidx)->array_index = i;
        }

        rd_i_listing_pop_indent(&b->listing, 1);
    }
    else if(rd_i_typedef_is_compound(tdef)) {
        rd_i_listing_push_indent(&b->listing, 1);

        usize memberidx = 0;
        RDParam* m;
        vect_each(m, &tdef->compound_) {
            LIndex midx = _rd_listing_process_type(b, &m->type, false);
            RDListingItem* litem = vect_at(&b->listing, midx);
            litem->parent_tdef = tdef;
            litem->member_index = memberidx++;
        }

        rd_i_listing_pop_indent(&b->listing, 1);
    }
    else
        b->address += rd_i_size_of(b->context, t->name, 0, t->flags);

    if(isroot) {
        vect_push(&b->listing.symbols, (RDSymbol){
                                           .kind = RD_SYMBOL_TYPE,
                                           .address = startaddr,
                                       });
    }

    return idx;
}

static void _rd_listing_process_data(RDListingBuilder* b) {
    usize index = rd_i_address2index(b->segment, b->address);

    if(rd_i_flagsbuffer_has_type(b->flags, index)) {
        RDTypeFull t;
        bool ok = rd_i_db_get_type(b->context, b->address, &t);
        panic_if(!ok, "type not found");
        RDListingModifier mod = _rd_listing_check_modifiers(b, index);
        LIndex lidx = _rd_listing_process_type(b, &t.base, true);
        vect_at(&b->listing, lidx)->mod = mod;
    }
    else
        panic("unhandled flag %08x", b->flags[index]);
}

static void _rd_listing_process_code(RDListingBuilder* b) {
    usize index = rd_i_address2index(b->segment, b->address);
    rd_i_listing_push_indent(&b->listing, 2);

    if(rd_i_flagsbuffer_has_func(b->flags, index)) {
        RDFunction* f = rd_i_function_create(b->context, b->address);
        vect_push(&b->listing.functions, f);

        vect_push(&b->listing.symbols, (RDSymbol){
                                           .kind = RD_SYMBOL_FUNCTION,
                                           .address = b->address,
                                       });

        rd_i_listing_pop_indent(&b->listing, 2);
        RDListingModifier mod = _rd_listing_check_modifiers(b, index);
        LIndex lidx = rd_i_listing_add_function(&b->listing, f);
        vect_at(&b->listing, lidx)->mod = mod;
        rd_i_listing_push_indent(&b->listing, 2);
    }
    else if(rd_i_flagsbuffer_has_xref_in(b->flags, index)) {
        rd_i_listing_pop_indent(&b->listing, 2);
        rd_i_listing_add_label(&b->listing, b->address);
        rd_i_listing_push_indent(&b->listing, 2);
    }

    rd_i_listing_add_instruction(&b->listing, b->address);

    usize len = rd_i_flagsbuffer_get_range_length(b->flags, index);
    panic_if(!len, "invalid code length");
    b->address += len;

    rd_i_listing_pop_indent(&b->listing, 2);
}

void rd_i_listing_build(RDContext* ctx) {
    RDListingBuilder b = {.context = ctx};
    rd_i_listing_init(&b.listing, &ctx->listing);

    RDSegmentFull** s;
    vect_each(s, &ctx->segments) {
        b.segment = *s;

        rd_i_listing_add_segment(&b.listing, b.segment);
        b.address = b.segment->base.start_address;
        b.flags = b.segment->flags;

        vect_push(&b.listing.symbols, (RDSymbol){
                                          .kind = RD_SYMBOL_SEGMENT,
                                          .address = b.address,
                                      });

        while(b.address < b.segment->base.end_address) {
            usize i = rd_i_address2index(b.segment, b.address);
            assert(!rd_i_flagsbuffer_has_tail(b.flags, i) && "tail detected");
            rd_i_listing_push_indent(&b.listing, 2);

            if(rd_i_flagsbuffer_has_unknown(b.flags, i))
                _rd_listing_process_unknown(&b);
            else if(rd_i_flagsbuffer_has_data(b.flags, i))
                _rd_listing_process_data(&b);
            else if(rd_i_flagsbuffer_has_code(b.flags, i))
                _rd_listing_process_code(&b);
            else
                unreachable();

            rd_i_listing_pop_indent(&b.listing, 2);
        }
    }

    RDFunction** f;
    vect_each(f, &b.listing.functions) {
        rd_i_function_build_graph(*f, &b.listing.chunks);
    }

    rd_i_functionchunk_sort(&b.listing.chunks);

    mem_swap(RDListing, &ctx->listing, &b.listing);
    rd_i_listing_deinit(&b.listing);
    LOG_INFO("listing generated with %d items", ctx->listing.length);
}
