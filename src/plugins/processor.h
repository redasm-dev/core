#pragma once

#include <redasm/plugins/processor/processor.h>

typedef struct RDContextProcessor {
    const RDProcessorPlugin* plugin;
    RDProcessor* instance;
} RDContextProcessor;

RDContextProcessor rd_i_processor_create(const RDProcessorPlugin* p);
void rd_i_processor_destroy(RDContextProcessor* self);

bool rd_i_processor_resolve(RDContext* ctx);
bool rd_i_processor_has_emulate(const RDContext* ctx);
bool rd_i_processor_has_lift(const RDContext* ctx);
bool rd_i_processor_has_render_segment(const RDContext* ctx);
bool rd_i_processor_has_render_function(const RDContext* ctx);
bool rd_i_processor_has_render_instruction(const RDContext* ctx);
void rd_i_processor_decode(const RDContext* ctx, RDInstruction* instr);
void rd_i_processor_emulate(RDContext* ctx, const RDInstruction* instr);
void rd_i_processor_lift(const RDContext* ctx, const RDInstruction* instr,
                         RDILInstruction* il);
const char* rd_i_processor_get_mnemonic(RDContext* ctx, const RDInstruction*);
const char* rd_i_processor_get_register(RDContext* ctx, int);
const char** rd_i_processor_get_prologues(RDContext* ctx);
void rd_i_processor_render_segment(RDContext* ctx, RDRenderer*,
                                   const RDSegment*);
void rd_i_processor_render_function(RDContext* ctx, RDRenderer*,
                                    const RDFunction*);
void rd_i_processor_render_instruction(RDContext* ctx, RDRenderer*,
                                       const RDInstruction*);
