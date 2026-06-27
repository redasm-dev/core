#include "analyzer.h"
#include "core/context.h"
#include "core/state.h"
#include "plugins/common.h"
#include "support/containers.h"
#include "support/logging.h"
#include <redasm/allocator.h>

static int _rd_analyzers_cmp(const void* arg1, const void* arg2) {
    const RDPlugin* a1 = *(const RDPlugin**)arg1;
    const RDPlugin* a2 = *(const RDPlugin**)arg2;
    if(a1->analyzer->order < a2->analyzer->order) return -1;
    if(a1->analyzer->order > a2->analyzer->order) return 1;
    return 0;
}

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

void rd_i_analyzeritemvect_destroy(RDAnalyzerItemVect* self) {
    RDAnalyzerItem** ai;
    vect_each(ai, self) rd_free(*ai);
    vect_destroy(self);
}

bool rd_register_analyzer(const RDAnalyzerPlugin* a) {
    if(!rd_i_validate_plugin_with_name(a->level, a->id, a->name, "analyzer"))
        return false;

    if(!a->execute) {
        LOG_FAIL("analyzer '%s' requires an executor", a->id);
        return false;
    }

    if(rd_analyzer_find(a->id)) {
        LOG_WARN("analyzer '%s' already registered", a->id);
        return false;
    }

    LOG_DEBUG("registering analyzer '%s' [%s]", a->id, a->name);
    RDPlugin* plugin = rd_alloc(sizeof(*plugin));
    plugin->analyzer = a;
    vect_push(&rd_i_state.analyzers, plugin);
    vect_sort(&rd_i_state.analyzers, _rd_analyzers_cmp);
    return true;
}
