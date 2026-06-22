#pragma once

#include <redasm/config.h>

bool rd_i_validate_plugin(u32 level, const char* id, const char* kind);
bool rd_i_validate_plugin_with_name(u32 level, const char* id, const char* name,
                                    const char* kind);
