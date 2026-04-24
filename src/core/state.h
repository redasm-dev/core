#pragma once

#include "plugins/module.h"
#include "support/utils.h"
#include "theme.h"
#include <redasm/redasm.h>

typedef struct RDPluginVect {
    RDPlugin** data;
    usize length;
    usize capacity;
} RDPluginVect;

typedef struct RDGlobalState {
    RDModule* modules;
    RDPluginVect loaders;
    RDPluginVect processors;
    RDPluginVect analyzers;
    RDPluginVect commands;
    RDTheme theme;
    RDCharVect fmt_buf;

    struct {
        RDContext** data;
        usize length;
        usize capacity;
    } tests;

} RDGlobalState;

extern RDGlobalState rd_i_state;

void rd_i_state_init(void);
void rd_i_state_deinit(void);
