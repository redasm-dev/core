#pragma once

#include <redasm/function.h>
#include <redasm/plugins/plugin.h>
#include <redasm/plugins/processor/instruction.h>
#include <redasm/plugins/processor/rdil.h>
#include <redasm/segment.h>
#include <redasm/surface/renderer.h>

typedef struct RDProcessor RDProcessor;

typedef enum {
    RD_PROCESSOR_LE = 0,
    RD_PROCESSOR_BE = (1 << 0),
} RDProcessorFlags;

// clang-format off
typedef struct RDProcessorPlugin {
    RD_PLUGIN_HEADER;
    void* userdata;
    unsigned int code_ptr_size;
    unsigned int ptr_size;
    unsigned int int_size;

    RDProcessor* (*create)(const struct RDProcessorPlugin*);
    void (*destroy)(RDProcessor*);

    void (*decode)(RDContext*, RDInstruction*, RDProcessor*);
    void (*emulate)(RDContext*, const RDInstruction*, RDProcessor*);
    void (*lift)(RDContext*, const RDInstruction*, RDILInstruction*, RDProcessor*);

    const char* (*get_mnemonic)(const RDInstruction*, RDProcessor*);
    const char* (*get_register)(int, RDProcessor*);
    const char** (*get_prologues)(RDProcessor*, RDContext*);

    void (*render_segment)(RDRenderer*, const RDSegment*, RDProcessor*);
    void (*render_function)(RDRenderer*, const RDFunction*, RDProcessor*);
    void (*render_instruction)(RDRenderer*, const RDInstruction*, RDProcessor*);
} RDProcessorPlugin;
// clang-format on

RD_API bool rd_register_processor(const RDProcessorPlugin* p);
