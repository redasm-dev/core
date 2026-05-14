#pragma once

#include "core/segment.h"
#include <redasm/context.h>
#include <redasm/registers.h>

typedef struct RDSegmentVect {
    RDSegmentFull** data;
    usize length;
    usize capacity;
} RDSegmentVect;

typedef struct RDMappingVect {
    RDInputMapping* data;
    usize length;
    usize capacity;
} RDMappingVect;

typedef struct RDSegmentRegVect {
    RDSegmentReg* data;
    usize length;
    usize capacity;

    const char* name;
} RDSegmentRegVect;

typedef struct RDSegmentRegsVect {
    RDSegmentRegVect* data;
    usize length;
    usize capacity;
} RDSegmentRegsVect;

typedef struct RDSegmentRegNameVect {
    const char** data;
    usize length;
    usize capacity;
} RDSegmentRegNameVect;

typedef struct RDName {
    const char* value;
    RDConfidence confidence;
} RDName;

typedef struct RDOvrOperand {
    RDAddress address;
    usize index;
} RDOvrOperand;

typedef struct RDOvrOperandVect {
    RDOvrOperand* data;
    usize length;
    usize capacity;
} RDOvrOperandVect;

typedef struct RDXRefVect {
    RDXRef* data;
    usize length;
    usize capacity;
} RDXRefVect;

int _rd_i_db_segment_cmp_pred(const void* a, const void* b);
int _rd_i_db_segment_find_pred(const void* key, const void* item);

int _rd_i_db_mapping_cmp_pred(const void* a, const void* b);
int _rd_i_db_mapping_find_pred(const void* key, const void* item);

int _rd_i_db_segmentreg_cmp(const void* a, const void* b);
