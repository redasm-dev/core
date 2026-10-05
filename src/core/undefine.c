#include "undefine.h"
#include "core/context.h"
#include "db/db.h"
#include "db/types.h"
#include "io/flagsbuffer.h"
#include "support/containers.h"
#include "support/error.h"
#include <inttypes.h>

#define RD_UNDEFINE_PUBLIC_MASK 0xFFFF

typedef struct RDUndefineRange {
    RDAddress start_address;
    RDAddress end_address; // exclusive
} RDUndefineRange;

typedef struct RDUndefineState {
    RDXRefVect xrefs;
    RDAddressVect q_targets; // jump targets of what was removed, still to visit
    RDConfidence confidence;
    RDUndefineFlags flags;
} RDUndefineState;

// a jump to one of these is not followed: the callee / the symbol stays
static bool _rd_is_root(const RDFlagsBuffer* flags, usize idx) {
    return rd_flagsbuffer_has_func(flags, idx) ||
           rd_flagsbuffer_has_exported(flags, idx) ||
           rd_flagsbuffer_has_imported(flags, idx);
}

static bool _rd_strip_flags(RDContext* self, const RDSegment* seg, usize idx,
                            RDUndefineState* state, bool apply) {
    if(!apply && state->confidence == RD_CONFIDENCE_MAX) return true;

    RDAddress address = rd_segment_get_start(seg) + idx;

    // outgoing xrefs are derived from what was decoded here
    if(rd_i_flagsbuffer_has_xref_out(seg->flags, idx)) {
        vect_clear(&state->xrefs);
        rd_i_db_get_xrefs_from(self, address, RD_XR_NONE, &state->xrefs);

        if(!apply) {
            const RDXRef* xref;
            vect_each(xref, &state->xrefs) {
                if(state->confidence < xref->confidence) return false;
            }
        }
        else if(vect_is_empty(&state->xrefs)) {
            // stale flag: no rows behind
            rd_i_flagsbuffer_clear_xref_out(seg->flags, idx);
        }
        else {
            const RDXRef* xref;
            vect_each(xref, &state->xrefs) {
                bool ok = rd_i_del_xref(self, address, xref->address,
                                        state->confidence);
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
    usize end = idx + len, seg_len = rd_flagsbuffer_get_length(seg->flags);
    if(end > seg_len) end = seg_len;

    for(usize i = idx; i < end; i++) {
        if(!_rd_strip_flags(self, seg, i, state, false)) return false;
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

    const RDXRef* xref;
    vect_each(xref, &state->xrefs) {
        if(state->confidence < xref->confidence) return false;
    }

    return true;
}

// jump targets of the instructions in [idx, end_idx). Collected while their
// xrefs still exist: the exec deletes them.
static void _rd_collect_jumps(RDContext* self, const RDSegment* seg, usize idx,
                              usize end_idx, RDUndefineState* state) {
    while(idx < end_idx) {
        if(!rd_flagsbuffer_has_code(seg->flags, idx)) {
            idx++;
            continue;
        }

        if(rd_i_flagsbuffer_has_xref_out(seg->flags, idx)) {
            vect_clear(&state->xrefs);
            rd_i_db_get_xrefs_from(self, rd_segment_get_start(seg) + idx,
                                   RD_CR_JUMP, &state->xrefs);

            const RDXRef* xref;
            vect_each(xref, &state->xrefs)
                vect_push(&state->q_targets, xref->address);
        }

        idx += rd_i_flagsbuffer_get_range_length(seg->flags, idx);
    }
}

static bool _rd_probe_undefine_range(RDContext* self, RDUndefineState* state,
                                     RDUndefineRange* r) {
    assert(state);
    assert(r);

    if(r->start_address >= r->end_address) return false;

    const RDSegment* seg = rd_i_db_find_segment(self, r->start_address);
    if(!seg) return false;

    if(r->end_address > rd_segment_get_end(seg))
        r->end_address = rd_segment_get_end(seg);

    usize idx = rd_i_address2index(seg, r->start_address);

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

        // restart from the root
        idx = root_addr - rd_segment_get_start(seg);
    }

    r->start_address = rd_segment_get_start(seg) + idx; // recompute start

    usize first = idx, end_idx = idx + (r->end_address - r->start_address);
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
          !_rd_is_root(seg->flags, idx)) {
        usize len = rd_i_flagsbuffer_get_range_length(seg->flags, idx);

        // protected: truncate, never fail (what was asked already passed)
        if(!_rd_probe_flags(self, seg, idx, len, state)) break;

        idx += len;
    }

    if(state->flags & RD_UNDEFINE_TRACE)
        _rd_collect_jumps(self, seg, first, idx, state); // NOLINT

    r->end_address = rd_segment_get_start(seg) + idx;
    return true;
}

static void _rd_exec_undefine_n(RDContext* self, RDUndefineState* state,
                                const RDUndefineRange* r) {
    const RDSegment* seg = rd_i_db_find_segment(self, r->start_address);
    assert(seg);

    RDAddress base = rd_segment_get_start(seg);
    usize idx = r->start_address - base, end_idx = r->end_address - base,
          i = idx;
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
        _rd_strip_flags(self, seg, i, state, true);

    // the diagnostics of code that no longer exists have no subject
    rd_i_db_del_problems_from(self, r->start_address, r->end_address);

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

// probe + exec of ONE range. false: the gate said no, nothing was touched.
static bool _rd_undefine_one(RDContext* self, RDUndefineState* state,
                             RDUndefineRange* r) {
    if(!_rd_probe_undefine_range(self, state, r)) return false;

    if(state->flags & RD_UNDEFINE_FLOW) {
        RD_LOG_DEBUG("undefining flow %" PRIX64 " - %" PRIX64, r->start_address,
                     r->end_address);
    }

    _rd_exec_undefine_n(self, state, r);
    return true;
}

// still something to remove?
// Undefined code is not code any more, so this is also what ends loops and
// jumps back into what was already removed.
static bool _rd_is_candidate(RDContext* self, RDAddress address) {
    const RDSegment* seg = rd_i_db_find_segment(self, address);
    if(!seg) return false;

    usize idx = rd_i_address2index(seg, address);
    if(!rd_flagsbuffer_has_code(seg->flags, idx)) return false;
    if(_rd_is_root(seg->flags, idx)) return false;

    // the location flowed in from the previous item.
    // If that one is still code, it stays, and so does this.
    if(idx > 0 && rd_flagsbuffer_has_flow(seg->flags, idx)) {
        usize prev_idx = idx - 1;
        rd_i_flagsbuffer_expand_tails(seg->flags, &prev_idx, NULL);
        if(rd_flagsbuffer_has_code(seg->flags, prev_idx)) return false;
    }

    return true;
}

bool rd_i_undefine_range(RDContext* self, RDAddress address, usize n,
                         RDConfidence c, RDUndefineFlags flags,
                         RDAddress* start, RDAddress* end) {
    // a rewind keeps the arrival bits; FLOW / TRACE remove code because
    // arrival is lost
    assert(!((flags & RD_UNDEFINE_FLOW) && (flags & RD_UNDEFINE_CLEAR)));

    // never leave the outputs undefined: on failure they hold the request
    if(start) *start = address;
    if(end) *end = address + n;

    RDUndefineState state = {.confidence = c, .flags = flags};
    RDUndefineRange req = {
        .start_address = address,
        .end_address = address + n,
    };

    // the request is all or nothing
    if(!_rd_undefine_one(self, &state, &req)) {
        vect_destroy(&state.q_targets);
        vect_destroy(&state.xrefs);
        return false;
    }

    if(start) *start = req.start_address;
    if(end) *end = req.end_address;

    // what it pulls in is best effort: a protected target is simply left alone
    while(!vect_is_empty(&state.q_targets)) {
        usize last = vect_length(&state.q_targets) - 1;
        RDAddress addr = *vect_at(&state.q_targets, last);
        vect_del(&state.q_targets, last, 1);

        if(!_rd_is_candidate(self, addr)) continue;

        RDUndefineRange r = {.start_address = addr, .end_address = addr + 1};
        _rd_undefine_one(self, &state, &r);
    }

    vect_destroy(&state.q_targets);
    vect_destroy(&state.xrefs);
    return true;
}

bool rd_auto_undefine(RDContext* self, RDAddress address, usize flags) {
    return rd_i_undefine(self, address, RD_CONFIDENCE_AUTO,
                         flags & RD_UNDEFINE_PUBLIC_MASK);
}

bool rd_library_undefine(RDContext* self, RDAddress address, usize flags) {
    return rd_i_undefine(self, address, RD_CONFIDENCE_LIBRARY,
                         flags & RD_UNDEFINE_PUBLIC_MASK);
}

bool rd_user_undefine(RDContext* self, RDAddress address, usize flags) {
    return rd_i_undefine(self, address, RD_CONFIDENCE_USER,
                         flags & RD_UNDEFINE_PUBLIC_MASK);
}

bool rd_auto_undefine_n(RDContext* self, RDAddress address, usize n,
                        usize flags) {
    return rd_i_undefine_n(self, address, n, RD_CONFIDENCE_AUTO,
                           flags & RD_UNDEFINE_PUBLIC_MASK);
}

bool rd_library_undefine_n(RDContext* self, RDAddress address, usize n,
                           usize flags) {
    return rd_i_undefine_n(self, address, n, RD_CONFIDENCE_LIBRARY,
                           flags & RD_UNDEFINE_PUBLIC_MASK);
}

bool rd_user_undefine_n(RDContext* self, RDAddress address, usize n,
                        usize flags) {
    return rd_i_undefine_n(self, address, n, RD_CONFIDENCE_USER,
                           flags & RD_UNDEFINE_PUBLIC_MASK);
}
