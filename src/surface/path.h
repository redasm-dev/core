#pragma once

#include "core/db/db.h"
#include "surface/renderer.h"
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
                                                LIndex start, RDRowVect* rows,
                                                RDContext* ctx);
