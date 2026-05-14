#include "object.h"
#include <redasm/kb.h>

usize rd_kbobject_get_length(const RDKBObject* self) {
    if(self->datum.type == TOML_ARRAY) return self->datum.u.arr.size;
    if(self->datum.type == TOML_TABLE) return self->datum.u.tab.size;
    return 0;
}

const char* rd_kbobject_get_str(RDKBObject* self, const char* key) {
    toml_datum_t v = toml_seek(self->datum, key);
    if(v.type != TOML_STRING) return NULL;
    return v.u.s;
}
