#pragma once

#include <redasm/common.h>
#include <redasm/plugins/plugin.h>

typedef enum {
    RD_CMDARG_VOID = 0,
    RD_CMDARG_BOOL,
    RD_CMDARG_INT,
    RD_CMDARG_UINT,
    RD_CMDARG_STRING,
    RD_CMDARG_OFFSET,
    RD_CMDARG_ADDRESS,
} RDCommandValueKind;

typedef struct RDCommandValue {
    RDCommandValueKind kind;

    union {
        u64 u;
        i64 i;
        const char* s;
        RDAddress addr;
        RDOffset off;
        bool b;
    };
} RDCommandValue;

typedef struct RDCommandParam {
    RDCommandValueKind kind;
    const char* name;
} RDCommandParam;

typedef struct RDCommandPlugin {
    RD_PLUGIN_HEADER;
    const RDCommandParam* params;
    RDCommandValue (*execute)(RDContext* ctx, const RDCommandValue* args);
} RDCommandPlugin;

RD_API bool rd_register_command(const RDCommandPlugin* c);
RD_API RDCommandValue rd_command_run(RDContext* ctx, const char* name,
                                     const RDCommandValue* args);
