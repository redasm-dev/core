#pragma once

#include <redasm/support/datum.h>
#include <tomlc17.h>

static inline const toml_datum_t* rd_i_datum_to_handle(const RDDatum* obj) {
    return (const toml_datum_t*)obj;
}

static inline const RDDatum* rd_i_datum_from_handle(const toml_datum_t* datum) {
    return (const RDDatum*)datum;
}

static inline toml_type_t rd_i_datum_handle_type(const RDDatum* obj) {
    return rd_i_datum_to_handle(obj)->type;
}

void rd_i_datum_destroy(RDDatum* self);
