#include "flags.h"
#include <assert.h>

// FL_FLOW: set on the DESTINATION instruction:
// - means "reached by fall-through from the previous instruction".
// - never set on function entries (FL_FUNC) or noreturn functions (FL_NORET)
// - both clear FL_FLOW automatically in their set functions.

// agnostic flags to be preserved during clears
static const RDFlags FL_VALUE = 1U << 8;
static const RDFlags FL_NAME = 1U << 9;
static const RDFlags FL_COMMENT = 1U << 10;
static const RDFlags FL_XREFOUT = 1U << 11;
static const RDFlags FL_XREFIN = 1U << 12;
static const RDFlags FL_EXPORTED = 1U << 13;
static const RDFlags FL_IMPORTED = 1U << 14;

// covers bits 0-13 (NOTE: always add the last flag above)
static const RDFlags FL_PRESERVE_MASK = (FL_IMPORTED << 1) - 1;
static const RDFlags FL_HAS_INFO = FL_PRESERVE_MASK & ~(FL_VALUE | 0xFF);

static const RDFlags FL_CODE = 1U << 15;
static const RDFlags FL_DATA = 1U << 16;
static const RDFlags FL_TAIL = 1U << 17;

// [CODE] bits
static const RDFlags FL_FLOW = 1U << 18;
static const RDFlags FL_JUMP = 1U << 19;
static const RDFlags FL_JMPDST = 1U << 20;
static const RDFlags FL_CALL = 1U << 21;
static const RDFlags FL_FUNC = 1U << 22;
static const RDFlags FL_NORET = 1U << 23;
static const RDFlags FL_COND = 1U << 24;
static const RDFlags FL_DSLOT = 1U << 25;
static const RDFlags FL_OPOVER = 1U << 26;

// [DATA] bits
static const RDFlags FL_TYPE = 1U << 18;

bool rd_i_flags_has_unknown(RDFlags self) {
    return !(self & (FL_CODE | FL_DATA | FL_TAIL));
}

bool rd_i_flags_has_info(RDFlags self) { return self & FL_HAS_INFO; }
bool rd_i_flags_has_code(RDFlags self) { return self & FL_CODE; }
bool rd_i_flags_has_data(RDFlags self) { return self & FL_DATA; }
bool rd_i_flags_has_tail(RDFlags self) { return self & FL_TAIL; }
bool rd_i_flags_has_name(RDFlags self) { return self & FL_NAME; }
bool rd_i_flags_has_comment(RDFlags self) { return self & FL_COMMENT; }
bool rd_i_flags_has_xref_out(RDFlags self) { return self & FL_XREFOUT; }
bool rd_i_flags_has_xref_in(RDFlags self) { return self & FL_XREFIN; }
bool rd_i_flags_has_imported(RDFlags self) { return self & FL_IMPORTED; }
bool rd_i_flags_has_exported(RDFlags self) { return self & FL_EXPORTED; }

bool rd_i_flags_has_flow(RDFlags self) {
    return (self & FL_CODE) && (self & FL_FLOW);
}

bool rd_i_flags_has_jump(RDFlags self) {
    return (self & FL_CODE) && (self & FL_JUMP);
}

bool rd_i_flags_has_jmpdst(RDFlags self) {
    return (self & FL_CODE) && (self & FL_JMPDST);
}

bool rd_i_flags_has_call(RDFlags self) {
    return (self & FL_CODE) && (self & FL_CALL);
}

bool rd_i_flags_has_func(RDFlags self) {
    return (self & FL_CODE) && (self & FL_FUNC);
}

bool rd_i_flags_has_noret(RDFlags self) {
    return rd_i_flags_has_func(self) && (self & FL_NORET);
}

bool rd_i_flags_has_cond(RDFlags self) {
    return (self & FL_CODE) && (self & FL_COND);
}

bool rd_i_flags_has_dslot(RDFlags self) {
    return (self & FL_CODE) && (self & FL_DSLOT);
}

bool rd_i_flags_has_op_over(RDFlags self) {
    return (self & FL_CODE) && (self & FL_OPOVER);
}

bool rd_i_flags_has_type(RDFlags self) {
    return (self & FL_DATA) && (self & FL_TYPE);
}

bool rd_i_flags_get_value(RDFlags self, u8* b) {
    if(self & FL_VALUE) {
        if(b) *b = self & 0xFF;
        return true;
    }

    return false;
}

void rd_i_flags_set_value(RDFlags* self, u8 b) {
    *self &= (u8)~0xFF;
    *self |= (FL_VALUE | b);
}

void rd_i_flags_set_code(RDFlags* self) {
    rd_i_flags_undefine(self);
    *self |= FL_CODE;
}

void rd_i_flags_set_data(RDFlags* self) {
    rd_i_flags_undefine(self);
    *self |= FL_DATA;
}

void rd_i_flags_set_tail(RDFlags* self) {
    rd_i_flags_undefine(self);
    *self = (*self & ~(FL_CODE | FL_DATA)) | FL_TAIL;
}

void rd_i_flags_set_flow(RDFlags* self) {
    assert(rd_i_flags_has_code(*self));

    // silently ignore if function entry or noreturn:
    // these break flow by definition
    if(rd_i_flags_has_func(*self) || rd_i_flags_has_noret(*self)) return;

    *self |= FL_FLOW;
}

void rd_i_flags_set_jump(RDFlags* self) {
    assert(rd_i_flags_has_code(*self));
    *self |= FL_JUMP;
}

void rd_i_flags_set_jmpdst(RDFlags* self) {
    assert(rd_i_flags_has_code(*self));
    *self |= FL_JMPDST;
}

void rd_i_flags_set_call(RDFlags* self) {
    assert(rd_i_flags_has_code(*self));
    *self |= FL_CALL;
}

void rd_i_flags_set_func(RDFlags* self) {
    assert(rd_i_flags_has_code(*self));
    *self |= FL_FUNC;
    *self &= ~FL_FLOW; // function entries break flow
}

void rd_i_flags_set_noret(RDFlags* self) {
    assert(rd_i_flags_has_func(*self));
    *self |= FL_NORET;
    *self &= ~FL_FLOW; // NORET and FLOW are mutually exclusive
}

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
    *self |= FL_TYPE;
}

void rd_i_flags_set_name(RDFlags* self) {
    assert(!rd_i_flags_has_tail(*self));
    *self |= FL_NAME;
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

void rd_i_flags_undefine(RDFlags* self) { *self &= FL_PRESERVE_MASK; }

void rd_i_flags_undefine_name(RDFlags* self) {
    assert(!rd_i_flags_has_tail(*self));
    *self &= ~FL_NAME;
}

void rd_i_flags_undefine_comment(RDFlags* self) {
    assert(!rd_i_flags_has_tail(*self));
    *self &= ~FL_COMMENT;
}

void rd_i_flags_undefine_xref_out(RDFlags* self) {
    assert(!rd_i_flags_has_tail(*self));
    *self &= ~FL_XREFOUT;
}

void rd_i_flags_undefine_xref_in(RDFlags* self) {
    assert(!rd_i_flags_has_tail(*self));
    *self &= ~FL_XREFIN;
}

void rd_i_flags_undefine_op_over(RDFlags* self) {
    assert(rd_i_flags_has_code(*self));
    *self &= ~FL_OPOVER;
}
