#include "pattern.h"
#include "io/flags.h"
#include "support/containers.h"
#include <ctype.h>

bool rd_pattern_compile(RDPattern* self, const char* s) {
    vect_clear(self);

    while(*s) {
        if(*s == ' ') {
            s++;
            continue;
        }

        if(s[0] == '?' && s[1] == '?') {
            vect_push(self, (RDPatternByte){.wildcard = true, .value = 0});
            s += 2;
            continue;
        }

        if(isxdigit((u8)s[0]) && isxdigit((u8)s[1])) {
            u8 hi =
                isdigit((u8)s[0]) ? s[0] - '0' : toupper((u8)s[0]) - 'A' + 10;
            u8 lo =
                isdigit((u8)s[1]) ? s[1] - '0' : toupper((u8)s[1]) - 'A' + 10;

            vect_push(self, (RDPatternByte){.wildcard = false,
                                            .value = (hi << 4) | lo});
            s += 2;
            continue;
        }

        vect_clear(self); // invalid character
        return false;
    }

    return true;
}

void rd_pattern_destroy(RDPattern* self) {
    if(self) vect_destroy(self);
}

bool rd_pattern_match(const RDPattern* self, const RDFlagsBuffer* flags,
                      usize idx) {
    if(!self->length || idx + self->length > flags->base.length) return false;

    for(usize i = 0; i < self->length; i++) {
        const RDPatternByte* pb = vect_at(self, i);
        if(pb->wildcard) continue;

        RDFlags f = rd_i_flagsbuffer_get(flags, idx + i);
        u8 v;
        if(!rd_i_flags_get_value(f, &v) || pb->value != v) return false;
    }

    return true;
}
