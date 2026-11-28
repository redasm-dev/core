#pragma once

#include <redasm/plugins/container.h>

RDContainer* rd_i_container_create(const RDContainerPlugin* plugin);
void rd_i_container_destroy(const RDContainerPlugin* plugin, RDContainer* ldr);
