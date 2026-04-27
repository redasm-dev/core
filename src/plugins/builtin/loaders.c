#include "loaders.h"
#include <redasm/redasm.h>

static bool _builtin_binary_parse(RDLoader* ldr, const RDLoaderRequest* req) {
    RD_UNUSED(ldr);
    RD_UNUSED(req);
    return true;
}

static bool _builtin_binary_load(RDLoader* ldr, RDContext* ctx) {
    RD_UNUSED(ldr);
    const RDReader* r = rd_get_input_reader(ctx);
    usize n = rd_reader_get_length(r);
    rd_map_segment_n(ctx, "BINARY", 0, n, RD_SP_RWX);
    rd_map_input_n(ctx, 0, 0, n);
    return true;
}

static const RDLoaderPlugin BUILTIN_BINARY = {
    .level = RD_API_LEVEL,
    .id = "builtin_binary",
    .name = "Binary",
    .flags = PF_LAST,
    .parse = _builtin_binary_parse,
    .load = _builtin_binary_load,
};

void rd_i_builtin_binary(void) { rd_register_loader(&BUILTIN_BINARY); }
