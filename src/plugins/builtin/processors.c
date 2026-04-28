#include "processors.h"
#include "plugins/builtin/builtin.h"
#include <redasm/redasm.h>

static void _null_decode(const RDContext* ctx, RDInstruction* instr,
                         RDProcessor* p) {
    RD_UNUSED(ctx);
    RD_UNUSED(instr);
    RD_UNUSED(p);
}

static void _null_emulate(RDContext* ctx, const RDInstruction* instr,
                          RDProcessor* p) {
    RD_UNUSED(ctx);
    RD_UNUSED(instr);
    RD_UNUSED(p);
}

static const RDProcessorPlugin BUILTIN_NULL = {
    .level = RD_API_LEVEL,
    .id = RD_NULL_PROCESSOR_ID,
    .flags = PF_LAST,
    .name = "Null",
    .int_size = sizeof(intmax_t),
    .ptr_size = sizeof(ptrdiff_t),
    .decode = _null_decode,
    .emulate = _null_emulate,
};

void rd_i_builtin_null(void) { rd_register_processor(&BUILTIN_NULL); }
