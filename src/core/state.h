#pragma once

#include "core/settings.h"
#include "net/http/http.h"
#include "net/socket/socket.h"
#include "plugins/loader/option.h"
#include "support/path.h"
#include "support/scratch.h"
#include "theme.h"
#include <redasm/redasm.h>

typedef struct RDModuleFull RDModuleFull;

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

#if defined(RD_HAS_NETWORK)
    bool is_network_enabled;
    RDNetHttp* net_http;
    RDNetSocket* net_socket;
#endif

    RDTheme theme;

    char* settings_filepath;
    RDSettings* settings;

    RDLogCallback log_callback;
    void* log_userdata;

    RDPathVect kb_paths;
    RDCharVect kb_path_buf;
    RDCharVect kb_schema_buf;

    RDCharVect fmt_buf;
    RDCharVect log_buf;
    RDCharVect scratch_buf;
    RDCharVect toml_key_buf;
    RDCharVect instr_text_buf;
    RDCharVect instr_dump_buf;
    RDCharVect mnem_buf;
    RDCharVect path_dirname_buf;
    RDCharVect path_stem_buf;
    RDCharVect path_join_buf;

    RDLoaderOptionVect optgroup_buf;
    RDScratchBuffer encode_buf;
    RDContext* encode_ctx;

    struct {
        RDTestResult** data;
        usize length;
        usize capacity;
    } tests;
} RDGlobalState;

extern RDGlobalState rd_i_state;

RDTestResult* rd_i_testresult_create(const RDLoaderPlugin* loaderplugin,
                                     const RDProcessorPlugin* processorplugin,
                                     RDByteBuffer* inputbuf,
                                     const char* fileapth);
void rd_i_testresult_destroy(RDTestResult* self);

void rd_i_state_init(const RDInitParams* params);
void rd_i_state_deinit(void);
