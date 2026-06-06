#pragma once

#include <redasm/plugins/analyzer.h>

typedef struct RDAnalyzerItem {
    const RDAnalyzerPlugin* plugin;
    bool is_selected;
    usize n_runs;
} RDAnalyzerItem;
