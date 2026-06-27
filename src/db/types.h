#pragma once

#include "core/segment.h"
#include <redasm/context.h>
#include <redasm/registers.h>
#include <redasm/types/type.h>

typedef struct RDAddressVect {
    RDAddress* data;
    usize length;
    usize capacity;
} RDAddressVect;

typedef struct RDSegmentFullVect {
    RDSegmentFull** data;
    usize length;
    usize capacity;
} RDSegmentFullVect;

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

typedef struct RDExternalVect {
    RDExternal* data;
    usize length;
    usize capacity;
} RDExternalVect;

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

typedef struct RDXRefFull {
    RDXRef base;
    RDAddress from_address;
    RDConfidence confidence;
} RDXRefFull;

typedef struct RDTypeVect {
    RDType* data;
    usize length;
    usize capacity;
} RDTypeVect;

int _rd_i_db_segment_cmp_pred(const void* a, const void* b);
int _rd_i_db_segment_find_pred(const void* key, const void* item);

int _rd_i_db_mapping_cmp_pred(const void* a, const void* b);
int _rd_i_db_mapping_find_pred(const void* key, const void* item);

int _rd_i_db_segmentreg_cmp(const void* a, const void* b);

RDSegmentRegVect* _rd_i_db_segmentregs_find_vect(RDSegmentRegsVect* self,
                                                 const char* reg);
RDSegmentRegVect* _rd_i_db_segmentregs_get_vect(RDSegmentRegsVect* self,
                                                RDSegmentRegNameVect* names,
                                                const char* reg);
