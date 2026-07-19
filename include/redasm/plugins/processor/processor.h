#pragma once

#include <redasm/function.h>
#include <redasm/plugins/plugin.h>
#include <redasm/plugins/processor/instruction.h>
#include <redasm/rdil/rdil.h>
#include <redasm/registers.h>
#include <redasm/segment.h>
#include <redasm/support/scratch.h>
#include <redasm/surface/renderer.h>

typedef struct RDProcessor RDProcessor;

typedef enum {
    RD_PF_LE = 0,
    RD_PF_BE = (1 << 0),
} RDProcessorFlags;

// clang-format off
typedef struct RDProcessorPlugin {
    RD_PLUGIN_HEADER;
    const char* name;
    const char* operand_sep;
    unsigned int code_ptr_size;
    unsigned int ptr_size;
    unsigned int int_size;

    RDProcessor* (*create)(const struct RDProcessorPlugin*);
    void (*destroy)(RDProcessor*);

    void (*decode)(RDContext*, RDInstruction*, RDProcessor*);
    bool (*encode)(RDContext*, RDAddress, const char*, RDScratchBuffer*, RDProcessor*);
    void (*emulate)(RDContext*, const RDInstruction*, RDProcessor*);
    void (*lift)(RDContext*, RDInstructionVect*, const RDInstruction*, RDProcessor*);

    const char* (*get_mnemonic)(const RDInstruction*, RDProcessor*);

    const char* (*get_reg_name)(RDReg, RDProcessor*);
    bool (*get_reg_mask)(const char* name, RDRegMask*, RDProcessor*);

    void (*render_segment)(RDRenderer*, const RDSegment*, RDProcessor*);
    void (*render_function)(RDRenderer*, const RDFunction*, RDProcessor*);
    bool (*render_mnemonic)(RDRenderer*, const RDInstruction*, RDProcessor*);
    bool (*render_operand)(RDRenderer*, const RDInstruction*, int, RDProcessor*);
} RDProcessorPlugin;
// clang-format on

RD_API bool rd_register_processor(const RDProcessorPlugin* p);
RD_API const char* rd_get_reg_name(const RDContext* ctx, RDReg r);
RD_API unsigned int rd_get_ptr_size(const RDContext* ctx);
RD_API unsigned int rd_get_code_ptr_size(const RDContext* ctx);
RD_API unsigned int rd_get_int_size(const RDContext* ctx);
