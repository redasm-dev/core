#include "support/error.h"
#include "support/logging.h"
#include <redasm/plugins/command.h>

static int _rd_command_count_params(const RDCommandPlugin* plugin) {
    const RDCommandParam* p = plugin->params;

    while(p->name && p->kind)
        p++;

    return p - plugin->params;
}

static int _rd_command_count_args(const RDCommandValue* args) {
    const RDCommandValue* a = args;

    while(a->kind != RD_CMDARG_VOID)
        a++;

    return a - args;
}

static const char* _rd_command_type_name(RDCommandValueKind kind) {
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

    unreachable();
    return NULL;
}

static bool _rd_command_validate_args(const RDCommandPlugin* p,
                                      const RDCommandParam* params,
                                      const RDCommandValue* args) {
    int i = 1;

    while(args->kind != RD_CMDARG_VOID) {
        if(params->kind != args->kind) {
            const char* paramtype = _rd_command_type_name(params->kind);
            const char* argtype = _rd_command_type_name(args->kind);

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
