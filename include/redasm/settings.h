#pragma once

#include <redasm/config.h>
#include <redasm/support/datum.h>

RD_API bool rd_settings_is_loaded(void);
RD_API const char* rd_settings_get_filepath(void);
RD_API const RDDatum* rd_settings_root(void);
