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
        RDKBOrdinalModule* data;
        usize length;
        usize capacity;
    } ordinal_modules;

    struct {
        const char** data;
        usize length;
        usize capacity;
    } noret_names;
} RDKB;

void rd_i_kb_paths_init(const char** kb_paths);
void rd_i_kb_paths_deinit(RDPathVect* self);

RDKB* rd_i_kb_create(void);
void rd_i_kb_destroy(RDKB* self);
void rd_i_kb_add_noret(RDContext* ctx, const char* name);
bool rd_i_kb_is_noret(const RDContext* ctx, const char* name);
const char* rd_i_kb_find_ordinal_name(RDContext* ctx, const char* module,
                                      u32 ordinal);
