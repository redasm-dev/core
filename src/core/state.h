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

    RDLogCallback log_callback;
    void* log_userdata;

    RDCharVect fmt_buf;
    RDCharVect log_buf;
    RDCharVect instr_text_buf;
    RDCharVect instr_dump_buf;
    RDCharVect mnem_buf;

    struct {
        RDContext** data;
        usize length;
        usize capacity;
    } tests;
} RDGlobalState;

extern RDGlobalState rd_i_state;

void rd_i_state_init(void);
void rd_i_state_deinit(void);
