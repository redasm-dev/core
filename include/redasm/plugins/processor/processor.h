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
    void* userdata;
    const char* parent;
    const char* operand_sep;
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
    const char** (*get_prologues)(RDProcessor*, const RDContext*);

    void (*render_segment)(RDRenderer*, const RDSegment*, RDProcessor*);
    void (*render_function)(RDRenderer*, const RDFunction*, RDProcessor*);
    void (*render_mnemonic)(RDRenderer*, const RDInstruction*, RDProcessor*);
    void (*render_operand)(RDRenderer*, const RDInstruction*, usize idx, RDProcessor*);
} RDProcessorPlugin;
// clang-format on

RD_API bool rd_register_processor(const RDProcessorPlugin* p);
RD_API u32 rd_processor_get_flags(const RDContext* ctx);
RD_API const char* rd_processor_get_id(const RDContext* ctx);
RD_API const char* rd_processor_get_name(const RDContext* ctx);
RD_API const char* rd_processor_get_operand_sep(const RDContext* ctx);
RD_API unsigned int rd_processor_get_code_ptr_size(const RDContext* ctx);
RD_API unsigned int rd_processor_get_ptr_size(const RDContext* ctx);
RD_API unsigned int rd_processor_get_int_size(const RDContext* ctx);

RD_API void rd_processor_parent_decode(const RDContext* ctx,
                                       RDInstruction* instr);
RD_API void rd_processor_parent_emulate(RDContext* ctx,
                                        const RDInstruction* instr);
RD_API void rd_processor_parent_lift(const RDContext* ctx,
                                     const RDInstruction* instr,
                                     RDILInstruction* il);
RD_API const char* rd_processor_parent_get_operand_sep(const RDContext* ctx);
RD_API const char* rd_processor_parent_get_mnemonic(const RDContext* ctx,
                                                    const RDInstruction* instr);
RD_API const char* rd_processor_parent_get_register(const RDContext* ctx,
                                                    int r);
RD_API const char** rd_processor_parent_get_prologues(const RDContext* ctx);
RD_API void rd_processor_parent_render_segment(RDContext* ctx, RDRenderer* r,
                                               const RDSegment* s);
RD_API void rd_processor_parent_render_function(RDContext* ctx, RDRenderer* r,
                                                const RDFunction* f);
RD_API void rd_processor_parent_render_mnemonic(RDContext* ctx, RDRenderer* r,
                                                const RDInstruction* instr);
RD_API void rd_processor_parent_render_operand(RDContext* ctx, RDRenderer* r,
                                               const RDInstruction* instr,
                                               usize idx);
