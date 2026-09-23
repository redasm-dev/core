#include "settings.h"
#include "core/state.h"
#include "support/datum.h"
#include <redasm/allocator.h>
#include <redasm/support/logging.h>
#include <redasm/support/utils.h>

typedef struct RDSettings { // box TOML backend
    toml_result_t toml;
} RDSettings;

RDSettings* rd_i_settings_create(const char* filepath) {
    if(!filepath) return NULL;

    toml_result_t toml = toml_parse_file_ex(filepath);

    if(!toml.ok) {
        RD_LOG_FAIL("%s", toml.errmsg);
        return NULL;
    }

    RDSettings* self = rd_alloc(sizeof(RDSettings));
    self->toml = toml;
    return self;
}

void rd_i_settings_destroy(RDSettings* self) {
    if(!self) return;

    toml_free(self->toml);
    rd_free(self);
}

bool rd_settings_is_loaded(void) { return !!rd_i_state.settings; }

const char* rd_settings_get_filepath(void) {
    return rd_i_state.settings_filepath;
}

const RDDatum* rd_settings_root(void) {
    return rd_i_state.settings
               ? rd_i_datum_from_handle(&rd_i_state.settings->toml.toptab)
               : NULL;
}
