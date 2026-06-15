#pragma once

#include "support/utils.h"
#include "theme.h"
#include <redasm/redasm.h>

typedef struct RDModuleFull RDModuleFull;

typedef struct RDTestResult {
    char* filepath;
    RDByteBuffer* input_buffer;
    const RDLoaderPlugin* loaderplugin;
    const RDProcessorPlugin* processorplugin;
    char* loader_name;
    RDLoader* loader;
    bool owns_loader;
} RDTestResult;

typedef struct RDPluginVect {
    RDPlugin** data;
    usize length;
    usize capacity;
} RDPluginVect;

typedef struct RDGlobalState {
    struct {
        RDModuleFull** data;
        usize length;
        usize capacity;
    } modules;

    RDPluginVect loaders;
    RDPluginVect processors;
    RDPluginVect analyzers;
    RDPluginVect commands;

    RDTheme theme;

    RDLogCallback log_callback;
    void* log_userdata;

    RDPathVect kb_paths;
    RDCharVect kb_path_buf;
    RDCharVect kb_schema_buf;
    RDCharVect kb_key_buf;

    RDCharVect fmt_buf;
    RDCharVect log_buf;
    RDCharVect instr_text_buf;
    RDCharVect instr_dump_buf;
    RDCharVect mnem_buf;

    struct {
        RDTestResult** data;
        usize length;
        usize capacity;
    } tests;
} RDGlobalState;

extern RDGlobalState rd_i_state;

RDTestResult* rd_i_testresult_create(const RDLoaderPlugin* lplugin,
                                     RDLoader* loader, RDByteBuffer* inputbuf,
                                     const char* fileapth);
void rd_i_testresult_destroy(RDTestResult* self);

void rd_i_state_init(const RDInitParams* params);
void rd_i_state_deinit(void);
