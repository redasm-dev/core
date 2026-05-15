#pragma once

#include <redasm/kb.h>
#include <tomlc17.h>

static inline const toml_datum_t* rd_i_kb_to_datum(const RDKBObject* obj) {
    return (const toml_datum_t*)obj;
}

static inline const RDKBObject* rd_i_kb_from_datum(const toml_datum_t* datum) {
    return (const RDKBObject*)datum;
}
