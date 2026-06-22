#pragma once

#include <redasm/plugins/analyzer.h>

typedef struct RDAnalyzerItem {
    const RDAnalyzerPlugin* plugin;
    bool is_selected;
    usize n_runs;
} RDAnalyzerItem;

typedef struct RDAnalyzerItemVect {
    RDAnalyzerItem** data;
    usize length;
    usize capacity;
} RDAnalyzerItemVect;

void rd_i_analyzeritemvect_destroy(RDAnalyzerItemVect* self);
