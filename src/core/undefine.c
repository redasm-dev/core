#include "undefine.h"
#include "core/context.h"
#include "db/db.h"
#include "io/flagsbuffer.h"
#include "support/containers.h"
#include "support/error.h"
#include <inttypes.h>

static RDConfidence _rd_get_max_xrefs_confidence(const RDXRefVect* xrefs) {
    RDConfidence c = RD_CONFIDENCE_PLACEHOLDER;

    const RDXRef* xref;
    vect_each(xref, xrefs) {
        if(xref->confidence > c) c = xref->confidence;
    }

    return c;
}

static bool _rd_flag_is_entity(const RDFlagsBuffer* flags, usize idx) {
    return rd_flagsbuffer_has_name(flags, idx) ||
           rd_flagsbuffer_has_exported(flags, idx) ||
           rd_flagsbuffer_has_imported(flags, idx);
}

static bool _rd_has_inbound_xrefs_in(RDContext* self, RDUndefineState* state,
                                     RDAddress addr) {
    vect_clear(&state->xrefs);
    rd_i_db_get_xrefs_to(self, addr, RD_XR_NONE, &state->xrefs);

    const RDXRef* x;
    vect_each(x, &state->xrefs) {
        if(x->address < state->start || x->address >= addr) return true;
    }

    return false;
}

static bool _rd_strip_flags(RDContext* self, const RDSegment* seg, usize idx,
                            RDAddress address, RDUndefineState* state,
                            bool apply) {
    if(!apply && state->confidence == RD_CONFIDENCE_MAX) return true;

    // outgoing xrefs are derived from what was decoded here, from any byte
    if(rd_i_flagsbuffer_has_xref_out(seg->flags, idx)) {
        vect_clear(&state->xrefs);
        rd_i_db_get_xrefs_from(self, address, RD_XR_NONE, &state->xrefs);

        if(!apply) {
            if(state->confidence < _rd_get_max_xrefs_confidence(&state->xrefs))
                return false;
        }
        else if(vect_is_empty(&state->xrefs)) {
            // no rows behind this flag: it is stale
            rd_i_flagsbuffer_clear_xref_out(seg->flags, idx);
        }
        else {
            const RDXRef* x;
            vect_each(x, &state->xrefs) {
                bool ok =
                    rd_i_del_xref(self, address, x->address, state->confidence);
                assert(ok && "undefine: the probe approved a protected xref");
                RD_UNUSED(ok);
            }
        }
    }

    if((state->flags & RD_UNDEFINE_NAMES) &&
       rd_flagsbuffer_has_name(seg->flags, idx)) {
        if(!apply) {
            RDName n;
            if(rd_i_db_get_name(self, address, &n) &&
               state->confidence < n.confidence)
                return false;
        }
        else {
            rd_i_db_del_name(self, address);
            rd_i_flagsbuffer_clear_name(seg->flags, idx);
        }
    }

    if(apply && (state->flags & RD_UNDEFINE_COMMENTS) &&
       rd_i_flagsbuffer_has_comment(seg->flags, idx)) {
        rd_i_db_del_all_comments(self, address);
        rd_i_flagsbuffer_clear_comment(seg->flags, idx);
    }

    // inbound xrefs belong to their referrers and stay, but the flag must match
    if(apply && rd_i_flagsbuffer_has_xref_in(seg->flags, idx) &&
       !rd_i_db_has_xrefs_to(self, address, RD_XR_NONE))
        rd_i_flagsbuffer_clear_xref_in(seg->flags, idx);

    return true;
}

// gate for [idx, idx + len) of one item
static bool _rd_probe_flags(RDContext* self, const RDSegment* seg, usize idx,
                            usize len, RDUndefineState* state) {
    RDAddress base = rd_segment_get_start(seg);
    usize end = idx + len, seg_len = rd_flagsbuffer_get_length(seg->flags);
    if(end > seg_len) end = seg_len;

    for(usize i = idx; i < end; i++) {
        if(!_rd_strip_flags(self, seg, i, base + i, state, false)) return false;
    }

    return true;
}

static bool _rd_probe_xrefs_in(RDContext* self, const RDSegment* seg, usize idx,
                               RDUndefineState* state) {
    if(state->confidence == RD_CONFIDENCE_MAX) return true;
    if(!rd_i_flagsbuffer_has_xref_in(seg->flags, idx)) return true;

    vect_clear(&state->xrefs);
    rd_i_db_get_xrefs_to(self, rd_segment_get_start(seg) + idx, RD_XR_NONE,
                         &state->xrefs);

    return state->confidence >= _rd_get_max_xrefs_confidence(&state->xrefs);
}

static bool _rd_probe_undefine_n(RDContext* self, RDUndefineState* state) {
    assert(state);

    const RDSegment* seg = rd_i_db_find_segment(self, state->start);
    if(!seg) return false;

    if(state->end > rd_segment_get_end(seg))
        state->end = rd_segment_get_end(seg);

    usize idx = rd_i_address2index(seg, state->start);

    // back to the head of its run
    rd_i_flagsbuffer_expand_tails(seg->flags, &idx, NULL);

    // inner head: field / item
    if(rd_flagsbuffer_has_data(seg->flags, idx) &&
       !rd_flagsbuffer_has_type(seg->flags, idx)) {
        RDAddress addr = rd_segment_get_start(seg) + idx;
        RDAddress root_addr = addr;
        RDType root;

        bool ok = rd_i_db_get_root_type(self, &root_addr, &root);
        panic_if(!ok || addr >= root_addr + rd_type_size(&root, self),
                 "inner head @ %" PRIX64 " is outside its nearest type", addr);

        idx = root_addr - rd_segment_get_start(seg); // restart from the root
    }

    // recompute start
    state->start = rd_segment_get_start(seg) + idx;

    usize end_idx = idx + (state->end - state->start);
    bool last_code = false;

    while(idx < end_idx) {
        RDAddress addr = rd_segment_get_start(seg) + idx;

        if(rd_flagsbuffer_has_code(seg->flags, idx)) {
            usize len = rd_i_flagsbuffer_get_range_length(seg->flags, idx);

            if(!_rd_probe_xrefs_in(self, seg, idx, state)) return false;
            if(!_rd_probe_flags(self, seg, idx, len, state)) return false;

            last_code = true;
            idx += len;
        }
        else if(rd_flagsbuffer_has_data(seg->flags, idx)) {
            if(rd_flagsbuffer_has_type(seg->flags, idx)) {
                RDType t;
                bool ok = rd_i_db_get_type(self, addr, &t);
                panic_if(!ok, "cannot get type @ %" PRIX64, addr);
                if(state->confidence < t.confidence) return false;

                usize sz = rd_type_size(&t, self);

                // inner heads own xrefs too
                if(!_rd_probe_flags(self, seg, idx, sz, state)) return false;

                last_code = false;
                idx += sz;
            }
            else
                unreachable();
        }
        else {
            panic_if(rd_flagsbuffer_has_tail(seg->flags, idx),
                     "tail detected %" PRIX64, addr);

            // unknown bytes can still carry names and comments
            if(!_rd_probe_flags(self, seg, idx, 1, state)) return false;

            last_code = false;
            idx++;
        }
    }

    usize seg_len = rd_flagsbuffer_get_length(seg->flags);
    if(idx > seg_len) idx = seg_len;

    // instructions that exist only because the range fell into them
    while((state->flags & RD_UNDEFINE_FLOW) && last_code && idx < seg_len &&
          rd_flagsbuffer_has_code(seg->flags, idx) &&
          rd_flagsbuffer_has_flow(seg->flags, idx) &&
          !_rd_flag_is_entity(seg->flags, idx)) {
        RDAddress addr = rd_segment_get_start(seg) + idx;
        usize len = rd_i_flagsbuffer_get_range_length(seg->flags, idx);

        // is 'addr' still reached by something outside [state->start, addr)?
        if(rd_i_flagsbuffer_has_xref_in(seg->flags, idx) &&
           _rd_has_inbound_xrefs_in(self, state, addr))
            break;

        // protected: truncate, never fail (what was asked already passed)
        if(!_rd_probe_flags(self, seg, idx, len, state)) break;

        idx += len;
    }

    state->end = rd_segment_get_start(seg) + idx;
    return true;
}

static void _rd_exec_undefine_n(RDContext* self, RDUndefineState* state) {
    const RDSegment* seg = rd_i_db_find_segment(self, state->start);
    assert(seg);

    RDAddress base = rd_segment_get_start(seg);
    usize idx = state->start - base, end_idx = state->end - base, i = idx;
    bool clear = state->flags & RD_UNDEFINE_CLEAR;

    // entity records need the flags that the wipe below drops
    while(i < end_idx) {
        if(!clear && rd_flagsbuffer_has_code(seg->flags, i)) {
            if(rd_flagsbuffer_has_func(seg->flags, i))
                rd_i_function_undeclare(self, seg, i);

            i += rd_i_flagsbuffer_get_range_length(seg->flags, i);
        }
        else if(rd_flagsbuffer_has_data(seg->flags, i)) {
            RDType t;
            bool ok = rd_i_db_get_type(self, base + i, &t);
            panic_if(!ok, "cannot get type @ %" PRIX64, base + i);

            rd_i_db_del_type(self, base + i);
            i += rd_type_size(&t, self);
        }
        else
            i++;
    }

    assert(i == end_idx && "undefine execution diverged from the probe");

    // the whole range becomes unknown bytes: no tails left from here on
    if(clear)
        rd_i_flagsbuffer_clear(seg->flags, idx, end_idx);
    else
        rd_i_flagsbuffer_undefine(seg->flags, idx, end_idx);

    for(i = idx; i < end_idx; i++)
        _rd_strip_flags(self, seg, i, base + i, state, true);

    // the diagnostics of code that no longer exists have no subject
    rd_i_db_del_problems_from(self, state->start, state->end);

    // the instruction after the range lost its predecessor:
    // it no longer falls into anything
    if(end_idx < rd_flagsbuffer_get_length(seg->flags) &&
       rd_flagsbuffer_has_code(seg->flags, end_idx) &&
       rd_flagsbuffer_has_flow(seg->flags, end_idx))
        rd_i_flagsbuffer_clear_flow(seg->flags, end_idx);
}

bool rd_i_undefine(RDContext* self, RDAddress address, RDConfidence c,
                   RDUndefineFlags flags) {
    return rd_i_undefine_n(self, address, 1, c, flags);
}

bool rd_i_undefine_n(RDContext* self, RDAddress address, usize n,
                     RDConfidence c, RDUndefineFlags flags) {
    return rd_i_undefine_range(self, address, n, c, flags, NULL, NULL);
}

bool rd_i_undefine_range(RDContext* self, RDAddress address, usize n,
                         RDConfidence c, RDUndefineFlags flags,
                         RDAddress* start, RDAddress* end) {
    RDUndefineState und_state = {
        .start = address,
        .end = address + n,
        .confidence = c,
        .flags = flags,
    };

    // never leave the outputs undefined: on failure they hold the request
    if(start) *start = address;
    if(end) *end = address + n;

    if(!_rd_probe_undefine_n(self, &und_state)) {
        vect_destroy(&und_state.xrefs);
        return false;
    }

    RD_LOG_DEBUG("undefining range %" PRIX64 " - %" PRIX64, und_state.start,
                 und_state.end);

    _rd_exec_undefine_n(self, &und_state);

    if(start) *start = und_state.start;
    if(end) *end = und_state.end;

    vect_destroy(&und_state.xrefs);
    return true;
}

bool rd_auto_undefine(RDContext* self, RDAddress address,
                      RDUndefineFlags flags) {
    return rd_i_undefine(self, address, RD_CONFIDENCE_AUTO,
                         flags & RD_UNDEFINE_PUBLIC_MASK);
}

bool rd_library_undefine(RDContext* self, RDAddress address,
                         RDUndefineFlags flags) {
    return rd_i_undefine(self, address, RD_CONFIDENCE_LIBRARY,
                         flags & RD_UNDEFINE_PUBLIC_MASK);
}

bool rd_user_undefine(RDContext* self, RDAddress address,
                      RDUndefineFlags flags) {
    return rd_i_undefine(self, address, RD_CONFIDENCE_USER,
                         flags & RD_UNDEFINE_PUBLIC_MASK);
}

bool rd_auto_undefine_n(RDContext* self, RDAddress address, usize n,
                        RDUndefineFlags flags) {
    return rd_i_undefine_n(self, address, n, RD_CONFIDENCE_AUTO,
                           flags & RD_UNDEFINE_PUBLIC_MASK);
}

bool rd_library_undefine_n(RDContext* self, RDAddress address, usize n,
                           RDUndefineFlags flags) {
    return rd_i_undefine_n(self, address, n, RD_CONFIDENCE_LIBRARY,
                           flags & RD_UNDEFINE_PUBLIC_MASK);
}

bool rd_user_undefine_n(RDContext* self, RDAddress address, usize n,
                        RDUndefineFlags flags) {
    return rd_i_undefine_n(self, address, n, RD_CONFIDENCE_USER,
                           flags & RD_UNDEFINE_PUBLIC_MASK);
}
