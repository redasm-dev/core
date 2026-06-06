#include "analyzer.h"
#include "core/context.h"
#include "support/containers.h"
#include <redasm/allocator.h>

static int _rd_analyzeritem_kcmp_pred(const void* key, const void* v) {
    const RDAnalyzerPlugin* plugin = (const RDAnalyzerPlugin*)key;
    const RDAnalyzerItem* item = (const RDAnalyzerItem*)v;

    if(plugin->order < item->plugin->order) return -1;
    if(plugin->order > item->plugin->order) return 1;
    return 0;
}

bool rd_analyzer_enable(RDContext* ctx, const char* id) {
    const RDAnalyzerPlugin* plugin = rd_analyzer_find(id);
    if(!plugin) return false;

    usize idx = vect_lower_bound(&ctx->analyzerplugins, plugin,
                                 _rd_analyzeritem_kcmp_pred);

    if(idx < vect_length(&ctx->analyzerplugins) &&
       !strcmp(id, (*vect_at(&ctx->analyzerplugins, idx))->plugin->id)) {
        return false;
    }

    RDAnalyzerItem* ai = rd_alloc(sizeof(*ai));

    *ai = (RDAnalyzerItem){
        .plugin = plugin,
        .is_selected = plugin->flags & RD_AF_SELECTED,
    };

    vect_ins(&ctx->analyzerplugins, idx, ai);
    return true;
}
