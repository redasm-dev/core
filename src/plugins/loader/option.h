#pragma once

#include "support/stringpool.h"
#include "support/utils.h"
#include <redasm/plugins/loader/option.h>

typedef struct RDLoaderOptionVect {
    RDLoaderOption* data;
    usize length;
    usize capacity;
} RDLoaderOptionVect;

typedef struct RDLoaderOptionBuilder {
    RDLoaderOptionVect options;
    RDStringVect groups;
    const char* current_group;
    RDStringPool strings;
} RDLoaderOptionBuilder;

void rd_i_loader_option_init(RDLoaderOptionBuilder* self);
void rd_i_loader_option_deinit(RDLoaderOptionBuilder* self);
