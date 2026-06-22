#include "core/state.h"
#include "plugins/common.h"
#include "support/containers.h"
#include "support/logging.h"
#include <redasm/plugins/command.h>

static int _rd_command_count_params(const RDCommandPlugin* plugin) {
    const RDCommandParam* p = plugin->params;

    while(p->name && p->kind)
        p++;

    return (int)(p - plugin->params);
}

static int _rd_command_count_args(const RDCommandValue* args) {
    const RDCommandValue* a = args;

    while(a->kind != RD_CMDARG_VOID)
        a++;

    return (int)(a - args);
}

static bool _rd_command_validate_args(const RDCommandPlugin* p,
                                      const RDCommandParam* params,
                                      const RDCommandValue* args) {
    int i = 1;

    while(args->kind != RD_CMDARG_VOID) {
        if(params->kind != args->kind) {
            const char* paramtype = rd_command_valuekind_str(params->kind);
            const char* argtype = rd_command_valuekind_str(args->kind);

            LOG_FAIL("command '%s', argument #%d: expects '%s', got '%s'",
                     p->id, i, paramtype, argtype);

            return false;
        }

        params++;
        args++;
        i++;
    }

    return true;
}

const char* rd_command_valuekind_str(RDCommandValueKind kind) {
    switch(kind) {
        case RD_CMDARG_VOID: return "VOID";
        case RD_CMDARG_BOOL: return "BOOL";
        case RD_CMDARG_INT: return "INT";
        case RD_CMDARG_UINT: return "UINT";
        case RD_CMDARG_STRING: return "STRING";
        case RD_CMDARG_OFFSET: return "OFFSET";
        case RD_CMDARG_ADDRESS: return "ADDRESS";
        default: break;
    }

    return "???";
}

RDCommandValue rd_command_run(RDContext* ctx, const char* name,
                              const RDCommandValue* args) {
    if(!name) {
        LOG_FAIL("command name is NULL");
        return (RDCommandValue){0};
    }

    if(!args) {
        LOG_FAIL("args is NULL for command '%s'", name);
        return (RDCommandValue){0};
    }

    const RDCommandPlugin* commandplugin = rd_command_find(name);

    if(!commandplugin) {
        LOG_FAIL("command '%s' not found", name);
        return (RDCommandValue){0};
    }

    int nparams = _rd_command_count_params(commandplugin);
    int nargs = _rd_command_count_args(args);

    if(nparams != nargs) {
        LOG_FAIL("expected '%d' arguments, got '%d' for command '%s'", nparams,
                 nargs, name);
        return (RDCommandValue){0};
    }

    if(!_rd_command_validate_args(commandplugin, commandplugin->params, args))
        return (RDCommandValue){0};

    return commandplugin->execute(ctx, args);
}

bool rd_register_command(const RDCommandPlugin* c) {
    if(!rd_i_validate_plugin_with_name(c->level, c->id, c->name, "command"))
        return false;

    if(!c->execute) {
        LOG_FAIL("command '%s' requires an executor", c->id);
        return false;
    }

    if(rd_command_find(c->id)) {
        LOG_WARN("command '%s' already registered", c->id);
        return false;
    }

    LOG_DEBUG("registering command '%s' [%s]", c->id, c->name);
    RDPlugin* plugin = rd_alloc(sizeof(*plugin));
    plugin->command = c;
    vect_push(&rd_i_state.commands, plugin);
    return true;
}
