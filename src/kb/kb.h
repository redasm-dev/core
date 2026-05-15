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

typedef struct RDKB {
    RDPathVect paths;
    RDKBFileVect files;

    RDCharVect schema_buf;
    RDCharVect path_buf;
    RDCharVect key_buf;
} RDKB;

void rd_i_kb_init(const char** kb_paths);
void rd_i_kb_deinit(RDKB* self);
