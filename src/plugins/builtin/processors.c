#include "processors.h"
#include "plugins/builtin/builtin.h"
#include <redasm/redasm.h>

static const RDProcessorPlugin BUILTIN_NULL = {
    .level = RD_API_LEVEL,
    .id = RD_NULL_PROCESSOR_ID,
    .flags = RD_PF_LAST,
    .name = "Null",
    .int_size = sizeof(intmax_t),
    .ptr_size = sizeof(ptrdiff_t),
};

void rd_i_builtin_null(void) { rd_register_processor(&BUILTIN_NULL); }
