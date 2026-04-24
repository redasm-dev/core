#include "listing.h"
#include "core/function.h"
#include "support/containers.h"

#define RD_LISTING_INDENT_BASE 2

static LIndex _rd_listing_add_item(RDListing* self, RDListingItemKind kind,
                                   RDAddress address) {
    LIndex idx = self->length;

    vect_push(self, (RDListingItem){
                        .kind = kind,
                        .address = address,
                        .indent = self->indent,
                    });

    return idx;
}

void rd_i_listing_init(RDListing* self, RDListing* prev) {
    self->indent = 0;
    if(!prev) return;

    vect_reserve(self, vect_capacity(prev));
    vect_reserve(&self->symbols, vect_capacity(&prev->symbols));
    vect_reserve(&self->exported, vect_capacity(&prev->exported));
    vect_reserve(&self->imported, vect_capacity(&prev->imported));
    vect_reserve(&self->functions, vect_capacity(&prev->functions));
    vect_reserve(&self->chunks, vect_capacity(&prev->chunks));
}

void rd_i_listing_deinit(RDListing* self) {
    rd_i_functionchunk_destroy(&self->chunks);

    RDFunction** f;
    vect_each(f, &self->functions) { rd_i_function_destroy(*f); }
    vect_destroy(&self->functions);

    vect_destroy(&self->symbols);
    vect_destroy(&self->exported);
    vect_destroy(&self->imported);
    vect_destroy(self);
}

void rd_i_listing_push_indent(RDListing* self, int c) {
    while(c--)
        self->indent += RD_LISTING_INDENT_BASE;
}

void rd_i_listing_pop_indent(RDListing* self, int c) {
    while(c--) {
        if(self->indent >= RD_LISTING_INDENT_BASE)
            self->indent -= RD_LISTING_INDENT_BASE;
        else
            break;
    }
}

LIndex rd_i_listing_add_segment(RDListing* self, const RDSegmentFull* s) {
    return _rd_listing_add_item(self, RD_LK_SEGMENT, s->base.start_address);
}

LIndex rd_i_listing_add_type(RDListing* self, RDAddress address,
                             const RDType* t) {
    LIndex idx = _rd_listing_add_item(self, RD_LK_TYPE, address);
    self->data[idx].type = *t;
    self->data[idx].array_index = RD_LISTING_NO_INDEX;
    self->data[idx].member_index = RD_LISTING_NO_INDEX;
    return idx;
}

LIndex rd_i_listing_add_hex_dump(RDListing* self, RDAddress startaddr,
                                 RDAddress endaddr) {
    LIndex idx = _rd_listing_add_item(self, RD_LK_HEX_DUMP, startaddr);
    self->data[idx].end_address = endaddr;
    return idx;
}

LIndex rd_i_listing_add_fill(RDListing* self, RDAddress startaddr,
                             RDAddress endaddr, RDListingFillByte fb) {
    LIndex idx = _rd_listing_add_item(self, RD_LK_FILL, startaddr);
    self->data[idx].end_address = endaddr;
    self->data[idx].fill = fb;
    return idx;
}

LIndex rd_i_listing_add_function(RDListing* self, const RDFunction* f) {
    LIndex idx = _rd_listing_add_item(self, RD_LK_FUNCTION, f->address);
    self->data[idx].func = f;
    return idx;
}

LIndex rd_i_listing_add_instruction(RDListing* self, RDAddress addr) {
    return _rd_listing_add_item(self, RD_LK_INSTRUCTION, addr);
}

LIndex rd_i_listing_add_label(RDListing* self, RDAddress addr) {
    return _rd_listing_add_item(self, RD_LK_LABEL, addr);
}

LIndex rd_i_listing_lower_bound(const RDListing* self, RDAddress address) {
    usize l = 0, r = self->length;

    while(l < r) {
        usize m = l + ((r - l) / 2);

        if(self->data[m].address < address)
            l = m + 1;
        else
            r = m;
    }

    return l;
}
