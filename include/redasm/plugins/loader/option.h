#pragma once

#include <redasm/common.h>
#include <redasm/config.h>

typedef struct RDLoaderOptionBuilder RDLoaderOptionBuilder;

typedef enum {
    RD_LOPT_BOOL = 0,
} RDLoaderOptionKind;

typedef struct RDLoaderOption {
    RDLoaderOptionKind kind;
    const char* id;
    const char* name;
    const char* desc;
    const char* group;
    bool defvalue;
    bool value;
} RDLoaderOption;

typedef struct RDLoaderOptionSlice {
    const RDLoaderOption* data;
    usize length;
} RDLoaderOptionSlice;

RD_API void rd_loader_options_set_group(RDLoaderOptionBuilder* self,
                                        const char* group);
RD_API void rd_loader_options_add_bool(RDLoaderOptionBuilder* self,
                                       const char* id, const char* name,
                                       const char* desc, bool v);
RD_API bool rd_get_loader_option_bool(const RDContext* ctx, const char* id,
                                      bool* v);
