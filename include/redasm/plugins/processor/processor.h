#pragma once

#include <redasm/function.h>
#include <redasm/plugins/plugin.h>
#include <redasm/plugins/processor/instruction.h>
#include <redasm/plugins/processor/rdil.h>
#include <redasm/registers.h>
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
    void* userdata;
    const char* operand_sep;
    unsigned int code_ptr_size;
    unsigned int ptr_size;
    unsigned int int_size;

    RDProcessor* (*create)(const struct RDProcessorPlugin*);
    void (*destroy)(RDProcessor*);

    void (*decode)(RDContext*, RDInstruction*, RDProcessor*);
    void (*emulate)(RDContext*, const RDInstruction*, RDProcessor*);
    void (*lift)(RDContext*, const RDInstruction*, RDILInstruction*, RDProcessor*);

    const char* (*get_mnemonic)(const RDInstruction*, RDProcessor*);
    const char* (*get_register_name)(RDReg, RDProcessor*);
    RDReg (*get_register_id)(const char*, RDProcessor*);
    const char** (*get_prologues)(RDProcessor*, const RDContext*);

    void (*render_segment)(RDRenderer*, const RDSegment*, RDProcessor*);
    void (*render_function)(RDRenderer*, const RDFunction*, RDProcessor*);
    void (*render_mnemonic)(RDRenderer*, const RDInstruction*, RDProcessor*);
    void (*render_operand)(RDRenderer*, const RDInstruction*, usize idx, RDProcessor*);
} RDProcessorPlugin;
// clang-format on

RD_API bool rd_register_processor(const RDProcessorPlugin* p);
RD_API const char* rd_get_register_name(const RDContext* ctx, RDReg r);
RD_API RDReg rd_get_register_id(const RDContext* ctx, const char* name);
