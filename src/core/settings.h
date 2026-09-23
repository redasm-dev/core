#pragma once

#include <redasm/settings.h>

typedef struct RDSettings RDSettings;

RDSettings* rd_i_settings_create(const char* filepath);
void rd_i_settings_destroy(RDSettings* self);
