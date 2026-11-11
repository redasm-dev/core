#include "core/state.h"
#include <redasm/settings.h>
#include <redasm/support/logging.h>
#include <redasm/support/utils.h>

bool rd_settings_is_loaded(void) { return !!rd_i_state.settings; }

const char* rd_settings_get_filepath(void) {
    return rd_i_state.settings_filepath;
}

const RDDatum* rd_settings_global(void) {
    return rd_datumdoc_get_root(rd_i_state.settings);
}
