#pragma once

#include "db/types.h"
#include "surface/row.h"
#include <redasm/surface/surface.h>

typedef struct RDSurfacePathVect {
    RDSurfacePathItem* data;
    usize length;
    usize capacity;
} RDSurfacePathVect;

typedef struct RDSurfacePath {
    RDSurfacePathVect path;
    RDXRefVect xrefs;
} RDSurfacePath;

void rd_i_surfacepath_deinit(RDSurfacePath* self);
const RDSurfacePathVect* rd_i_surfacepath_build(RDSurfacePath* self,
                                                const RDRowVect* rows,
                                                RDContext* ctx);
