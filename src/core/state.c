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
#include <redasm/plugins/processor/instruction.h>
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
    vect_destroy(&rd_i_state.log_buf);
    vect_destroy(&rd_i_state.fmt_buf);
    vect_destroy(&rd_i_state.instr_text_buf);
    vect_destroy(&rd_i_state.instr_dump_buf);
    vect_destroy(&rd_i_state.mnem_buf);
    _rd_i_state_unload_modules();
}

void rd_set_log_callback(RDLogCallback cb, void* userdata) {
    rd_i_state.log_callback = cb;
    rd_i_state.log_userdata = userdata;
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

const char* rd_dump_instruction(const RDInstruction* instr) {
    RDCharVect* d = &rd_i_state.instr_dump_buf;
    str_clear(d);
    if(!instr) return d->data;

    RDCharVect buf = {0};
    str_append(d, rd_i_format(&buf, "Address: %x\n", instr->address));
    str_append(d, rd_i_format(&buf, "Id: %x\n", instr->id));
    str_append(d, "Flow: ");

    switch(instr->flow) {
        case RD_IF_JUMP: str_append(d, "IF_JUMP"); break;
        case RD_IF_JUMP_COND: str_append(d, "IF_JUMP_COND"); break;
        case RD_IF_CALL: str_append(d, "IF_CALL"); break;
        case RD_IF_CALL_COND: str_append(d, "IF_CALL_COND"); break;
        case RD_IF_STOP: str_append(d, "IF_STOP"); break;
        case RD_IF_NOP: str_append(d, "IF_NOP"); break;
        default: str_append(d, "IF_NONE"); break;
    }

    if(rd_is_delay_slot(instr)) str_append(d, " | IS_DSLOT");
    str_push(d, '\n');

    int c = 0;
    while(c < RD_MAX_OPERANDS && instr->operands[c].kind != RD_OP_NULL)
        c++;

    str_append(d, rd_i_format(&buf, "N. Operands: %x\n", c));

    rd_foreach_operand(i, op, instr) {
        str_append(d, rd_i_format(&buf, "  [%d].kind: ", i));

        if(op->kind == RD_OP_REG) {
            str_append(d, "OP_REG\n");
            str_append(d, rd_i_format(&buf, "  [%d].reg: %x\n", i, op->reg));
        }
        else if(op->kind == RD_OP_IMM) {
            str_append(d, "OP_IMM\n");
            str_append(d, rd_i_format(&buf, "  [%d].imm: %x\n", i, op->imm));
        }
        else if(op->kind == RD_OP_ADDR) {
            str_append(d, "OP_ADDR\n");
            str_append(d, rd_i_format(&buf, "  [%d].addr: %x\n", i, op->addr));
        }
        else if(op->kind == RD_OP_MEM) {
            str_append(d, "OP_MEM\n");
            str_append(d, rd_i_format(&buf, "  [%d].mem: %x\n", i, op->mem));
        }
        else if(op->kind == RD_OP_PHRASE) {
            str_append(d, "OP_PHRASE\n");
            str_append(
                d, rd_i_format(&buf, "  [%d].base: %x\n", i, op->phrase.base));
            str_append(d, rd_i_format(&buf, "  [%d].index: %x\n", i,
                                      op->phrase.index));
        }
        else if(op->kind == RD_OP_DISPL) {
            str_append(d, "OP_DISPL\n");
            str_append(
                d, rd_i_format(&buf, "  [%d].base: %x\n", i, op->displ.base));
            str_append(
                d, rd_i_format(&buf, "  [%d].index: %x\n", i, op->displ.index));
            str_append(
                d, rd_i_format(&buf, "  [%d].displ: %x\n", i, op->displ.displ));
            str_append(
                d, rd_i_format(&buf, "  [%d].scale: %x\n", i, op->displ.scale));
        }
        else {
            str_append(d, rd_i_format(&buf, "%d", op->kind));
            if(op->kind >= RD_OP_USERBASE) str_append(d, " (USER)");
            str_push(d, '\n');
        }
    }

    vect_destroy(&buf);
    return d->data;
}
