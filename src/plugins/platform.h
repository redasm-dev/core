#pragma once

#include <redasm/plugins/platform.h>

RDPlatform* rd_i_platform_create(const RDPlatformPlugin* plugin);
void rd_i_platform_destroy(const RDPlatformPlugin* plugin, RDPlatform* p);
