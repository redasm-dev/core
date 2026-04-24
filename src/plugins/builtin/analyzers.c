#include "analyzers.h"
#include <redasm/redasm.h>

static bool _rd_autorename_is_enabled(const RDAnalyzerPlugin* plugin) {
    RD_UNUSED(plugin);
    return true;
}

static void _rd_autorename_execute(RDContext* ctx) {}

static const RDAnalyzerPlugin BUILTIN_AUTORENAME = {
    .level = RD_API_LEVEL,
    .id = "autorename",
    .name = "Autorename Nullsubs and Thunks",
    .flags = PF_LAST | RD_AF_SELECTED,
    .order = UINT32_MAX,
    .is_enabled = _rd_autorename_is_enabled,
    .execute = _rd_autorename_execute,
};

void rd_i_builtin_autorename(void) {
    rd_register_analyzer(&BUILTIN_AUTORENAME);
}
