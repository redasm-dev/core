#pragma once

#include <redasm/config.h>
#include <redasm/support/datum.h>

typedef struct RDSettings RDSettings;

RD_API RDSettings* rd_settings_load(const char* filepath);
RD_API const RDDatum* rd_settings_get_root(const RDSettings* self);
RD_API void rd_settings_destroy(RDSettings* self);

RD_API bool rd_settings_is_loaded(void);
RD_API const char* rd_settings_get_filepath(void);
RD_API const RDDatum* rd_settings_global(void);
