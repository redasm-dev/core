#include "flags.h"
#include <assert.h>

/*
 * RDFlags: 32 bits per address.
 *
 * 31     27 26          22 21 20 19    17 16          9 8 7        0
 * ┌────────┬──────────────┬─────┬────────┬────────────┬─┬──────────┐
 * │  free  │  structure   │class│arrival │  preserve  │V│  value   │
 * └────────┴──────────────┴─────┴────────┴────────────┴─┴──────────┘
 *  └───── cleared by clear ─────┘ └────── survives clear ─────────┘
 *  └────── cleared by undefine ──────────┘ └─ survives undefine ──┘
 *
 * Ordering is forced by the masks: a flag survives an operation if it sits
 * BELOW that operation's cut. Both masks cut below the class bits, so both
 * wipe the whole structure region wholesale: neither needs to know which
 * namespace was in use. Only the has_* accessors gate on class.
 *
 * The three tiers, by who owns the truth:
 *   preserve   knowledge: users, loaders, DB rows. Class-agnostic and
 *              head-only: FL_NORET marks an instruction, a function EP or an
 *              IAT slot alike, exactly like FL_IMPORTED. Never retracted
 *              implicitly.
 *   arrival    how control REACHES here (call/jump/fall-through). Owned by the
 *              referrer, not by this cell's decode: a byte change here does not
 *              invalidate them, so they survive clear + re-decode. Dropped by
 *              undefine ("means nothing") or by becoming a TAIL ("cannot live
 *              mid-item").
 *   structure  what these bytes DECODE TO. Re-derived on every decode.
 *
 * FL_FLOW and FL_FUNC are mutually exclusive, enforced symmetrically by both
 * setters so the outcome does not depend on discovery order.
 * This is a structural backstop: a function entry is never a fall-through
 * target, which keeps graphs correct even when KB is missing.
 *
 * FL_FLOW: set on the DESTINATION instruction:
 * - means "reached by fall-through from the previous instruction".
 * - never set on function entries (FL_FUNC) or noreturn functions (FL_NORET)
 * - both clear FL_FLOW automatically in their set functions.
 */

// [VALUE]
// bits 0-7: byte value
static const RDFlags FL_VALUE = 1U << 8;

// [PRESERVE]
static const RDFlags FL_NAME = 1U << 9;
static const RDFlags FL_PATCH = 1U << 10;
static const RDFlags FL_COMMENT = 1U << 11;
static const RDFlags FL_XREFOUT = 1U << 12;
static const RDFlags FL_XREFIN = 1U << 13;
static const RDFlags FL_EXPORTED = 1U << 14;
static const RDFlags FL_IMPORTED = 1U << 15;
static const RDFlags FL_NORET = 1U << 16;

// covers bits 0-15 (NOTE: always add the last flag above)
#define FL_UNDEFINE_MASK ((RDFlags)((FL_NORET << 1) - 1))
#define FL_HAS_INFO                                                            \
    ((RDFlags)(FL_UNDEFINE_MASK & ~(FL_VALUE | FL_PATCH | 0xFF)))

// [ARRIVAL]
static const RDFlags FL_FLOW = 1U << 17;
static const RDFlags FL_JMPDST = 1U << 18;
static const RDFlags FL_FUNC = 1U << 19;

// covers bits 0-17 (NOTE: always add the last flag above)
#define FL_CLEAR_MASK ((RDFlags)((FL_FUNC << 1) - 1))

// [CLASS]: 2-bit field, bits 20-21
#define FL_CLASS_SHIFT 20
#define FL_CLASS_MASK ((RDFlags)(3U << FL_CLASS_SHIFT))

typedef enum {
    FL_CL_UNKNOWN = 0,
    FL_CL_CODE,
    FL_CL_DATA,
    FL_CL_TAIL,
} RDFlagsClass;

// [STRUCTURE] CODE
// kind: 2-bit field, bits 22-23.
// JUMP/CALL are mutually exclusive.
#define FL_KIND_SHIFT 22
#define FL_KIND_MASK ((RDFlags)(3U << FL_KIND_SHIFT))

typedef enum {
    FL_KI_NORMAL = 0,
    FL_KI_JUMP,
    FL_KI_CALL,
} RDFlagsKind;

static const RDFlags FL_COND = 1U << 24;
static const RDFlags FL_DSLOT = 1U << 25;
static const RDFlags FL_OPOVER = 1U << 26;

// [STRUCTURE] DATA
static const RDFlags FL_TYPE = 1U << 22;
static const RDFlags FL_FIELD = 1U << 23;
static const RDFlags FL_ITEM = 1U << 24;

static RDFlagsClass _rd_flags_class(RDFlags self) {
    return (RDFlagsClass)((self & FL_CLASS_MASK) >> FL_CLASS_SHIFT);
}

bool rd_i_flags_has_unknown(RDFlags self) {
    return _rd_flags_class(self) == FL_CL_UNKNOWN;
}

bool rd_i_flags_has_code(RDFlags self) {
    return _rd_flags_class(self) == FL_CL_CODE;
}

bool rd_i_flags_has_data(RDFlags self) {
    return _rd_flags_class(self) == FL_CL_DATA;
}

bool rd_i_flags_has_tail(RDFlags self) {
    return _rd_flags_class(self) == FL_CL_TAIL;
}

// arrival: no class gate, must read while cleared
bool rd_i_flags_has_flow(RDFlags self) { return self & FL_FLOW; }
bool rd_i_flags_has_jmpdst(RDFlags self) { return self & FL_JMPDST; }
bool rd_i_flags_has_func(RDFlags self) { return self & FL_FUNC; }

static RDFlagsKind _rd_flags_kind(RDFlags s) {
    if(!rd_i_flags_has_code(s)) return FL_KI_NORMAL;
    return (RDFlagsKind)((s & FL_KIND_MASK) >> FL_KIND_SHIFT);
}

bool rd_i_flags_has_info(RDFlags self) { return self & FL_HAS_INFO; }
bool rd_i_flags_has_name(RDFlags self) { return self & FL_NAME; }
bool rd_i_flags_has_patch(RDFlags self) { return self & FL_PATCH; }
bool rd_i_flags_has_comment(RDFlags self) { return self & FL_COMMENT; }
bool rd_i_flags_has_xref_out(RDFlags self) { return self & FL_XREFOUT; }
bool rd_i_flags_has_xref_in(RDFlags self) { return self & FL_XREFIN; }
bool rd_i_flags_has_imported(RDFlags self) { return self & FL_IMPORTED; }
bool rd_i_flags_has_exported(RDFlags self) { return self & FL_EXPORTED; }

bool rd_i_flags_has_jump(RDFlags self) {
    return _rd_flags_kind(self) == FL_KI_JUMP;
}

bool rd_i_flags_has_call(RDFlags self) {
    return _rd_flags_kind(self) == FL_KI_CALL;
}

bool rd_i_flags_has_noret(RDFlags self) { return self & FL_NORET; }

bool rd_i_flags_has_cond(RDFlags self) {
    return rd_i_flags_has_code(self) && (self & FL_COND);
}

bool rd_i_flags_has_dslot(RDFlags self) {
    return rd_i_flags_has_code(self) && (self & FL_DSLOT);
}

bool rd_i_flags_has_op_over(RDFlags self) {
    return rd_i_flags_has_code(self) && (self & FL_OPOVER);
}

bool rd_i_flags_has_type(RDFlags self) {
    return rd_i_flags_has_data(self) && (self & FL_TYPE);
}

bool rd_i_flags_has_field(RDFlags self) {
    return rd_i_flags_has_data(self) && (self & FL_FIELD);
}

bool rd_i_flags_has_item(RDFlags self) {
    return rd_i_flags_has_data(self) && (self & FL_ITEM);
}

bool rd_i_flags_get_value(RDFlags self, u8* b) {
    if(self & FL_VALUE) {
        if(b) *b = self & 0xFF;
        return true;
    }

    return false;
}

static void _rd_flags_set_class(RDFlags* self, RDFlagsClass c) {
    *self = (*self & ~FL_CLASS_MASK) | ((RDFlags)c << FL_CLASS_SHIFT);
}

static void _rd_flags_set_kind(RDFlags* self, RDFlagsKind k) {
    *self = (*self & ~FL_KIND_MASK) | ((RDFlags)k << FL_KIND_SHIFT);
}

void rd_i_flags_set_value(RDFlags* self, u8 b) {
    *self &= ~(RDFlags)0xFF;
    *self |= (FL_VALUE | b);
}

void rd_i_flags_set_code(RDFlags* self) {
    *self &= FL_CLEAR_MASK;
    _rd_flags_set_class(self, FL_CL_CODE);
}

void rd_i_flags_set_data(RDFlags* self) {
    *self &= FL_UNDEFINE_MASK;
    _rd_flags_set_class(self, FL_CL_DATA);
}

void rd_i_flags_set_tail(RDFlags* self) {
    *self &= FL_UNDEFINE_MASK;
    _rd_flags_set_class(self, FL_CL_TAIL);
}

void rd_i_flags_set_jump(RDFlags* self) {
    assert(rd_i_flags_has_code(*self));
    _rd_flags_set_kind(self, FL_KI_JUMP);
}

void rd_i_flags_set_call(RDFlags* self) {
    assert(rd_i_flags_has_code(*self));
    _rd_flags_set_kind(self, FL_KI_CALL);
}

void rd_i_flags_set_flow(RDFlags* self) {
    assert(!rd_i_flags_has_tail(*self));

    // FL_FUNC breaks flow by definition (function entries are not fallthrough)
    if(rd_i_flags_has_func(*self)) return;

    *self |= FL_FLOW;
}

void rd_i_flags_set_jmpdst(RDFlags* self) {
    assert(!rd_i_flags_has_tail(*self));
    *self |= FL_JMPDST;
}

void rd_i_flags_set_func(RDFlags* self) {
    assert(!rd_i_flags_has_tail(*self));
    *self |= FL_FUNC;
    *self &= ~FL_FLOW; // function entries break flow
}

void rd_i_flags_set_noret(RDFlags* self) { *self |= FL_NORET; }

void rd_i_flags_set_cond(RDFlags* self) {
    assert(rd_i_flags_has_jump(*self) || rd_i_flags_has_call(*self));
    *self |= FL_COND;
}

void rd_i_flags_set_dslot(RDFlags* self) {
    assert(rd_i_flags_has_code(*self));
    *self |= FL_DSLOT;
}

void rd_i_flags_set_op_over(RDFlags* self) {
    assert(rd_i_flags_has_code(*self));
    *self |= FL_OPOVER;
}

void rd_i_flags_set_type(RDFlags* self) {
    assert(rd_i_flags_has_data(*self));
    assert(!rd_i_flags_has_field(*self));
    *self |= FL_TYPE;
}

void rd_i_flags_set_field(RDFlags* self) {
    assert(rd_i_flags_has_data(*self));
    assert(!rd_i_flags_has_type(*self));
    *self |= FL_FIELD;
}

void rd_i_flags_set_item(RDFlags* self) {
    assert(rd_i_flags_has_data(*self));
    assert(!rd_i_flags_has_type(*self));
    *self |= FL_ITEM;
}

void rd_i_flags_set_name(RDFlags* self) {
    assert(!rd_i_flags_has_tail(*self));
    *self |= FL_NAME;
}

void rd_i_flags_set_patch(RDFlags* self) {
    assert(rd_i_flags_get_value(*self, NULL));
    *self |= FL_PATCH;
}

void rd_i_flags_set_comment(RDFlags* self) {
    assert(!rd_i_flags_has_tail(*self));
    *self |= FL_COMMENT;
}

void rd_i_flags_set_xref_out(RDFlags* self) {
    assert(!rd_i_flags_has_tail(*self));
    *self |= FL_XREFOUT;
}

void rd_i_flags_set_xref_in(RDFlags* self) {
    assert(!rd_i_flags_has_tail(*self));
    *self |= FL_XREFIN;
}

void rd_i_flags_set_imported(RDFlags* self) {
    assert(!rd_i_flags_has_tail(*self));
    *self |= FL_IMPORTED;
}

void rd_i_flags_set_exported(RDFlags* self) {
    assert(!rd_i_flags_has_tail(*self));
    *self |= FL_EXPORTED;
}

void rd_i_flags_undefine(RDFlags* self) { *self &= FL_UNDEFINE_MASK; }
void rd_i_flags_clear(RDFlags* self) { *self &= FL_CLEAR_MASK; }

void rd_i_flags_clear_name(RDFlags* self) {
    assert(!rd_i_flags_has_tail(*self));
    *self &= ~FL_NAME;
}

void rd_i_flags_clear_patch(RDFlags* self) {
    assert(rd_i_flags_get_value(*self, NULL));
    *self &= ~FL_PATCH;
}

void rd_i_flags_clear_func(RDFlags* self) {
    assert(!rd_i_flags_has_tail(*self));
    *self &= ~FL_FUNC;
}

void rd_i_flags_clear_comment(RDFlags* self) {
    assert(!rd_i_flags_has_tail(*self));
    *self &= ~FL_COMMENT;
}

void rd_i_flags_clear_xref_out(RDFlags* self) {
    assert(!rd_i_flags_has_tail(*self));
    *self &= ~FL_XREFOUT;
}

void rd_i_flags_clear_xref_in(RDFlags* self) {
    assert(!rd_i_flags_has_tail(*self));
    *self &= ~FL_XREFIN;
}

void rd_i_flags_clear_flow(RDFlags* self) {
    assert(rd_i_flags_has_code(*self));
    *self &= ~FL_FLOW;
}

void rd_i_flags_clear_op_over(RDFlags* self) {
    assert(rd_i_flags_has_code(*self));
    *self &= ~FL_OPOVER;
}
