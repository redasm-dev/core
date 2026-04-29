#pragma once

#include <redasm/function.h>
#include <redasm/plugins/plugin.h>
#include <redasm/plugins/processor/instruction.h>
#include <redasm/plugins/processor/rdil.h>
#include <redasm/segment.h>
#include <redasm/surface/renderer.h>

typedef struct RDProcessor RDProcessor;

typedef enum {
    RD_PF_LE = 0,
    RD_PF_BE = (1 << 0),
} RDProcessorFlags;

// clang-format off
typedef struct RDProcessorPlugin {
    RD_PLUGIN_HEADER;
    const char* parent;
    void* userdata;
    unsigned int code_ptr_size;
    unsigned int ptr_size;
    unsigned int int_size;

    RDProcessor* (*create)(const struct RDProcessorPlugin*);
    void (*destroy)(RDProcessor*);

    void (*decode)(const RDContext*, RDInstruction*, RDProcessor*);
    void (*emulate)(RDContext*, const RDInstruction*, RDProcessor*);
    void (*lift)(const RDContext*, const RDInstruction*, RDILInstruction*, RDProcessor*);

    const char* (*get_mnemonic)(const RDInstruction*, RDProcessor*);
    const char* (*get_register)(int, RDProcessor*);
    const char** (*get_prologues)(RDProcessor*, RDContext*);

    void (*render_segment)(RDRenderer*, const RDSegment*, RDProcessor*);
    void (*render_function)(RDRenderer*, const RDFunction*, RDProcessor*);
    void (*render_instruction)(RDRenderer*, const RDInstruction*, RDProcessor*);
} RDProcessorPlugin;
// clang-format on

RD_API bool rd_register_processor(const RDProcessorPlugin* p);
RD_API u32 rd_processor_get_flags(const RDContext* ctx);
RD_API const char* rd_processor_get_id(const RDContext* ctx);
RD_API const char* rd_processor_get_name(const RDContext* ctx);
RD_API unsigned int rd_processor_get_code_ptr_size(const RDContext* ctx);
RD_API unsigned int rd_processor_get_ptr_size(const RDContext* ctx);
RD_API unsigned int rd_processor_get_int_size(const RDContext* ctx);
