#include "path.h"
#include "core/context.h"
#include "io/flagsbuffer.h"
#include "support/containers.h"

static bool _rd_surfacepath_exists(const RDSurfacePath* self, int fromrow,
                                   int torow) {
    const RDSurfacePathItem* item;
    vect_each(item, &self->path) {
        if(item->from_row == fromrow && item->to_row == torow) return true;
    }
    return false;
}

static int _rd_surfacepath_calculate_index(LIndex start, RDRowVect* rows,
                                           RDContext* ctx, RDAddress address) {
    const RDListing* listing = &ctx->listing;

    const RDListingItem* first = vect_at(listing, start);
    if(address < first->address) return -1;

    const RDListingItem* last = vect_at(listing, start + vect_length(rows) - 1);
    if(address > last->address) return (int)vect_length(rows) + 1;

    int residx = -1;

    for(usize i = vect_length(rows); i-- > 0;) {
        const RDListingItem* item = vect_at(listing, start + i);

        if(item->address == address) {
            residx = (int)i;
            if(item->kind == RD_LK_INSTRUCTION) break;
        }

        if(item->address < address) break;
    }

    return residx;
}

static void _rd_surfacepath_insert(RDSurfacePath* self, RDContext* ctx,
                                   LIndex start, int fromrow, int torow) {
    if(fromrow == torow || _rd_surfacepath_exists(self, fromrow, torow)) return;

    bool is_dst_cond = false;
    const RDListing* listing = &ctx->listing;
    LIndex src_lidx = start + (LIndex)fromrow;

    if(fromrow >= 0 && src_lidx < vect_length(listing)) {
        const RDListingItem* src_item = vect_at(listing, src_lidx);
        const RDSegmentFull* seg = rd_i_find_segment(ctx, src_item->address);

        if(seg) {
            usize idx = rd_i_address2index(seg, src_item->address);
            is_dst_cond = rd_flagsbuffer_has_cond(seg->flags, idx);
        }
    }

    bool is_loop = fromrow > torow;
    RDThemeKind style;

    if(is_loop)
        style = RD_THEME_MUTED;
    else
        style = is_dst_cond ? RD_THEME_JUMP_COND : RD_THEME_JUMP;

    vect_push(&self->path, ((RDSurfacePathItem){
                               .from_row = fromrow,
                               .to_row = torow,
                               .is_loop = is_loop,
                               .style = style,
                           }));
}

void rd_i_surfacepath_deinit(RDSurfacePath* self) {
    vect_destroy(&self->xrefs);
    vect_destroy(&self->path);
}

const RDSurfacePathVect* rd_i_surfacepath_build(RDSurfacePath* self,
                                                LIndex start, RDRowVect* rows,
                                                RDContext* ctx) {
    const RDListing* listing = &ctx->listing;
    vect_clear(&self->path);

    for(usize i = 0; i < vect_length(rows); i++) {
        LIndex lidx = start + i;
        if(lidx >= vect_length(listing)) break;

        const RDListingItem* item = vect_at(listing, lidx);
        if(item->kind != RD_LK_INSTRUCTION) continue;

        const RDSegmentFull* seg = rd_i_find_segment(ctx, item->address);
        assert(seg);

        usize idx = rd_i_address2index(seg, item->address);
        bool b = rd_i_flagsbuffer_has_jmpdst(seg->flags, idx);

        if(b) {
            const RDXRefVect* xrefs = rd_i_get_xrefs_to_type_ex(
                ctx, item->address, RD_CR_JUMP, &self->xrefs);

            const RDXRef* r;
            vect_each(r, xrefs) {
                const RDSegmentFull* rseg = rd_i_find_segment(ctx, r->address);
                if(!rseg || !(rseg->base.perm & RD_SP_X)) continue;

                usize ridx = rd_i_address2index(rseg, r->address);
                if(!rd_flagsbuffer_has_code(rseg->flags, ridx)) continue;

                int fromrow = _rd_surfacepath_calculate_index(start, rows, ctx,
                                                              r->address);
                _rd_surfacepath_insert(self, ctx, start, fromrow, (int)i);
            }
        }

        if(rd_flagsbuffer_has_jump(seg->flags, idx)) {
            const RDXRefVect* xrefs = rd_i_get_xrefs_from_type_ex(
                ctx, item->address, RD_CR_JUMP, &self->xrefs);

            const RDXRef* r;
            vect_each(r, xrefs) {
                const RDSegmentFull* rseg = rd_i_find_segment(ctx, r->address);
                if(!rseg || !(rseg->base.perm & RD_SP_X)) continue;

                int torow = _rd_surfacepath_calculate_index(start, rows, ctx,
                                                            r->address);
                _rd_surfacepath_insert(self, ctx, start, (int)i, torow);
            }
        }
    }

    return &self->path;
}
