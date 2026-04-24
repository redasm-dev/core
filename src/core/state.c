#include "state.h"
#include "plugins/builtin/analyzers.h"
#include "plugins/builtin/loaders.h"
#include "plugins/builtin/processors.h"
#include "plugins/module.h"
#include "support/containers.h"
#include "support/logging.h"
#include <redasm/plugins/analyzer.h>
#include <redasm/plugins/command.h>
#include <redasm/plugins/loader.h>
#include <redasm/plugins/processor/processor.h>
#include <string.h>

RDGlobalState rd_i_state = {0};

static void _rd_i_state_unload_modules(void) {
    RDModule* m = rd_i_state.modules;

    while(m) {
        RDModule* n = m->next;
        if(m->destroy) m->destroy();
        rd_i_module_destroy(m);
        m = n;
    }
}

static RDModule* _rd_module_find(const char* filepath) {
    RDModule* m;
    list_each(m, rd_i_state.modules) {
        if(!strcmp(m->path, filepath)) return m;
    }

    return NULL;
}

void rd_i_state_init(void) {
    rd_i_theme_init(&rd_i_state.theme);
    rd_i_builtin_binary();
    rd_i_builtin_null();
    rd_i_builtin_autorename();
}

void rd_i_state_deinit(void) {
    RDPlugin** it;
    vect_each(it, &rd_i_state.analyzers) { free(*it); }
    vect_each(it, &rd_i_state.processors) { free(*it); }
    vect_each(it, &rd_i_state.loaders) { free(*it); }
    vect_each(it, &rd_i_state.commands) { free(*it); }

    vect_destroy(&rd_i_state.commands);
    vect_destroy(&rd_i_state.analyzers);
    vect_destroy(&rd_i_state.processors);
    vect_destroy(&rd_i_state.loaders);
    vect_destroy(&rd_i_state.fmt_buf);
    _rd_i_state_unload_modules();
}

const RDLoaderPlugin* rd_loader_find(const char* id) {
    if(!id) return NULL;

    RDPlugin** it;
    vect_each(it, &rd_i_state.loaders) {
        if(!strcmp((*it)->loader->id, id)) return (*it)->loader;
    }

    return NULL;
}

const RDProcessorPlugin* rd_processor_find(const char* id) {
    if(!id) return NULL;

    RDPlugin** it;
    vect_each(it, &rd_i_state.processors) {
        if(!strcmp((*it)->processor->id, id)) return (*it)->processor;
    }

    return NULL;
}

const RDAnalyzerPlugin* rd_analyzer_find(const char* id) {
    if(!id) return NULL;

    RDPlugin** it;
    vect_each(it, &rd_i_state.analyzers) {
        if(!strcmp((*it)->analyzer->id, id)) return (*it)->analyzer;
    }

    return NULL;
}

const RDCommandPlugin* rd_command_find(const char* id) {
    if(!id) return NULL;

    RDPlugin** it;
    vect_each(it, &rd_i_state.commands) {
        if(!strcmp((*it)->command->id, id)) return (*it)->command;
    }

    return NULL;
}

bool rd_module_load(const char* filepath) {
    RDModule* m = _rd_module_find(filepath);

    if(m) {
        LOG_WARN("module '%s' already loaded, skipping...", filepath);
        return true;
    }

    m = rd_i_module_create(filepath);

    if(m) {
        list_push(rd_i_state.modules, m);
        m->create();
        return true;
    }

    return false;
}

RDPluginSlice rd_get_all_loader_plugins(void) {
    return (RDPluginSlice){
        .data = (const RDPlugin**)rd_i_state.loaders.data,
        .length = rd_i_state.loaders.length,
    };
}

RDPluginSlice rd_get_all_processor_plugins(void) {
    return (RDPluginSlice){
        .data = (const RDPlugin**)rd_i_state.processors.data,
        .length = rd_i_state.processors.length,
    };
}

RDPluginSlice rd_get_all_analyzers_plugins(void) {
    return (RDPluginSlice){
        .data = (const RDPlugin**)rd_i_state.analyzers.data,
        .length = rd_i_state.analyzers.length,
    };
}

RDPluginSlice rd_get_all_command_plugins(void) {
    return (RDPluginSlice){
        .data = (const RDPlugin**)rd_i_state.commands.data,
        .length = rd_i_state.commands.length,
    };
}
