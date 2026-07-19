#include "path.h"
#include "core/context.h"
#include "io/flagsbuffer.h"
#include "support/containers.h"

/*
 * Jump-arrow polylines for the current viewport, computed directly from
 * the rendered rows + flags + xrefs.
 * Row indices are the GUI coordinate system; two sentinels are part of the
 * contract with the drawing code:
 *
 *   -1              the endpoint sits above the viewport
 *   row_count + 1   the endpoint sits below the viewport
 *
 * so partial arrows enter/exit at the edges.
 */

static bool _rd_surfacepath_exists(const RDSurfacePath* self, int fromrow,
                                   int torow) {
    const RDSurfacePathItem* item;

    vect_each(item, &self->path) {
        if(item->from_row == fromrow && item->to_row == torow) return true;
    }

    return false;
}

/*
 * Is this row the INSTRUCTION row of a CODE head?
 * A code head can also carry label and noret-comment rows, arrows attach to the
 * instruction.
 * Which sub_line that is comes from the single slot-layout source
 * in row.c (rd_i_code_instr_sub_line).
 */
static bool _rd_row_is_instruction(RDContext* ctx, const RDRow* row,
                                   const RDSegmentFull** out_seg,
                                   usize* out_idx) {
    if(row->sub_line == RD_SUB_LINE_NONE) return false; // segment banner

    const RDSegmentFull* seg = rd_i_db_find_segment(ctx, row->address);
    if(!seg) return false;

    usize idx = rd_i_address2index(seg, row->address);
    if(!rd_flagsbuffer_has_code(seg->flags, idx)) return false;
    if(row->sub_line != rd_i_row_code_instr_sub_line(seg, idx)) return false;

    if(out_seg) *out_seg = seg;
    if(out_idx) *out_idx = idx;
    return true;
}

/*
 * Map an address to its row in the viewport, preferring the instruction
 * row among the rows at that address (label rows share the address).
 * Relies on row addresses being monotonically nondecreasing, which holds
 * because segments are stored in ascending address order.
 */
static int _rd_surfacepath_calculate_row(const RDRowVect* rows, RDContext* ctx,
                                         RDAddress address) {
    if(vect_is_empty(rows)) return -1;

    if(address < vect_first(rows)->address) return -1;
    if(address > vect_last(rows)->address) return (int)vect_length(rows) + 1;

    int residx = -1;

    for(usize i = vect_length(rows); i-- > 0;) {
        const RDRow* row = vect_at(rows, i);

        if(row->address == address) {
            residx = (int)i;
            if(_rd_row_is_instruction(ctx, row, NULL, NULL)) break;
        }

        if(row->address < address) break;
    }

    return residx;
}

static void _rd_surfacepath_insert(RDSurfacePath* self, RDContext* ctx,
                                   const RDRowVect* rows, int fromrow,
                                   int torow) {
    if(fromrow == torow || _rd_surfacepath_exists(self, fromrow, torow)) return;

    bool is_dst_cond = false;

    if(fromrow >= 0 && (usize)fromrow < vect_length(rows)) {
        const RDRow* src = vect_at(rows, (usize)fromrow);
        const RDSegmentFull* seg = rd_i_db_find_segment(ctx, src->address);

        if(seg) {
            usize idx = rd_i_address2index(seg, src->address);
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
                                                const RDRowVect* rows,
                                                RDContext* ctx) {
    vect_clear(&self->path);

    for(usize i = 0; i < vect_length(rows); i++) {
        const RDRow* row = vect_at(rows, i);

        const RDSegmentFull* seg;
        usize idx;
        if(!_rd_row_is_instruction(ctx, row, &seg, &idx)) continue;

        if(rd_i_flagsbuffer_has_jmpdst(seg->flags, idx)) {
            const RDXRefVect* xrefs = rd_i_get_xrefs_to_ex(
                ctx, row->address, RD_CR_JUMP, &self->xrefs);

            const RDXRef* r;
            vect_each(r, xrefs) {
                const RDSegmentFull* rseg =
                    rd_i_db_find_segment(ctx, r->address);
                if(!rseg || !(rseg->base.perm & RD_SP_X)) continue;

                usize ridx = rd_i_address2index(rseg, r->address);
                if(!rd_flagsbuffer_has_code(rseg->flags, ridx)) continue;

                int fromrow =
                    _rd_surfacepath_calculate_row(rows, ctx, r->address);
                _rd_surfacepath_insert(self, ctx, rows, fromrow, (int)i);
            }
        }

        if(rd_flagsbuffer_has_jump(seg->flags, idx)) {
            const RDXRefVect* xrefs = rd_i_get_xrefs_from_ex(
                ctx, row->address, RD_CR_JUMP, &self->xrefs);

            const RDXRef* r;
            vect_each(r, xrefs) {
                const RDSegmentFull* rseg =
                    rd_i_db_find_segment(ctx, r->address);
                if(!rseg || !(rseg->base.perm & RD_SP_X)) continue;

                int torow =
                    _rd_surfacepath_calculate_row(rows, ctx, r->address);
                _rd_surfacepath_insert(self, ctx, rows, (int)i, torow);
            }
        }
    }

    return &self->path;
}
