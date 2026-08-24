#pragma once

#include "support/utils.h"
#include <redasm/kb.h>
#include <tomlc17.h>

typedef struct RDKBFile {
    char* name;
    toml_result_t toml;
    const RDKBObject* root;
} RDKBFile;

typedef struct RDKBFileVect {
    RDKBFile** data;
    usize length;
    usize capacity;
} RDKBFileVect;

typedef struct RDKBOrdinal {
    u32 ordinal;
    const char* name;
} RDKBOrdinal;

typedef struct RDKBOrdinalModule {
    const char* module;

    struct {
        RDKBOrdinal* data;
        usize length;
        usize capacity;
    } ordinals;
} RDKBOrdinalModule;

typedef struct RDKB {
    RDKBFileVect files;

    struct {
        const char** data;
        usize length;
        usize capacity;
    } curr_callconv;

    struct {
        RDKBOrdinalModule* data;
        usize length;
        usize capacity;
    } ordinal_modules;
} RDKB;

void rd_i_kb_paths_init(const char** kb_paths);
void rd_i_kb_paths_deinit(RDPathVect* self);

RDKB* rd_i_kb_create(void);
void rd_i_kb_destroy(RDKB* self);
const char* rd_i_kb_find_ordinal_name(RDContext* ctx, const char* module,
                                      u32 ordinal);
