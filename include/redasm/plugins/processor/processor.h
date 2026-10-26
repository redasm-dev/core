#pragma once

#include <redasm/function/function.h>
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

typedef struct RDRegisterDef {
    RDReg id;
    const char* name;
    RDReg root; // RD_REGID_INVALID: this register IS a root (full width)
    u8 bits;    // width of this view inside 'root' (unused for roots)
    u8 shift;   // position of this view inside 'root' (unused for roots)
} RDRegisterDef;

typedef enum {
    RD_QUERY_REG_BY_ID = 0,
    RD_QUERY_REG_BY_NAME,
} RDQueryRegKind;

typedef enum {
    RD_QUERY_REG_WANT_NONE = 0,
    RD_QUERY_REG_WANT_MASK = 1 << 0,
    RD_QUERY_REG_WANT_ROOT = 1 << 1,
} RDQueryRegWant;

typedef struct RDQueryReg {
    RDQueryRegKind kind;
    RDQueryRegWant want;
    RDReg id;
    const char* name;
    const char* root_name;
    RDRegMask mask;
} RDQueryReg;

// clang-format off
typedef struct RDProcessorPlugin {
    RD_PLUGIN_HEADER

    const char* name;
    const char* operand_sep;
    unsigned int code_ptr_size;
    unsigned int ptr_size;
    usize instance_size;

    RDProcessor* (*create)(const struct RDProcessorPlugin*);
    void (*destroy)(RDProcessor*);
    void (*setup)(RDContext*, RDProcessor*);

    void (*decode)(RDContext*, RDInstruction*, RDProcessor*);
    bool (*encode)(RDContext*, RDAddress, const char*, RDScratchBuffer*, RDProcessor*);
    void (*emulate)(RDContext*, const RDInstruction*, RDProcessor*);
    void (*lift)(RDContext*, RDInstructionVect*, const RDInstruction*, RDProcessor*);

    const char* (*get_mnemonic)(const RDInstruction*, RDProcessor*);
    bool (*query_reg)(RDQueryReg*, RDProcessor*);

    void (*render_segment)(RDRenderer*, const RDSegment*, RDProcessor*);
    void (*render_function)(RDRenderer*, const RDFunction*, RDProcessor*);
    bool (*render_mnemonic)(RDRenderer*, const RDInstruction*, RDProcessor*);
    bool (*render_operand)(RDRenderer*, const RDInstruction*, int, RDProcessor*);
} RDProcessorPlugin;
// clang-format on

RD_API bool rd_register_processor(const RDProcessorPlugin* p);
RD_API bool rd_query_reg(const RDContext* ctx, RDQueryReg* q);
RD_API bool rd_query_reg_table(RDQueryReg* q, const RDRegisterDef* defs);
RD_API unsigned int rd_get_ptr_size(const RDContext* ctx);
RD_API unsigned int rd_get_code_ptr_size(const RDContext* ctx);
