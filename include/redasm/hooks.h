#pragma once

#include <redasm/config.h>
#include <redasm/context.h>
#include <redasm/plugins/processor/instruction.h>
#include <redasm/surface/renderer.h>

typedef void (*RDInstructionHook)(RDContext*, RDInstruction*);
typedef void (*RDAddressHook)(RDContext*, RDAddress);
typedef void (*RDXRefHook)(RDContext*, RDAddress from, RDAddress to,
                           RDXRefType type);
typedef void (*RDRenderMnemonicHook)(RDContext*, RDRenderer*,
                                     const RDInstruction*);
typedef void (*RDRenderOperandHook)(RDContext*, RDRenderer*,
                                    const RDInstruction*, usize idx);

RD_API bool rd_register_instruction_hook(RDContext* ctx, const char* name,
                                         RDInstructionHook h);
RD_API bool rd_register_address_hook(RDContext* ctx, const char* name,
                                     RDAddressHook h);
RD_API bool rd_register_xref_hook(RDContext* ctx, const char* name,
                                  RDXRefHook h);
RD_API bool rd_register_render_mnemonic_hook(RDContext* ctx, const char* name,
                                             RDRenderMnemonicHook h);
RD_API bool rd_register_render_operand_hook(RDContext* ctx, const char* name,
                                            RDRenderOperandHook h);

RD_API void rd_fire_instruction_hook(RDContext* ctx, const char* name,
                                     RDInstruction*);
RD_API void rd_fire_address_hook(RDContext* ctx, const char* name,
                                 RDAddress addr);
RD_API void rd_fire_xref_hook(RDContext* ctx, const char* name, RDAddress from,
                              RDAddress to, RDXRefType type);
RD_API bool rd_fire_render_mnemonic_hook(RDContext*, const char* name,
                                         RDRenderer* r,
                                         const RDInstruction* instr);
RD_API bool rd_fire_render_operand_hook(RDContext*, const char* name,
                                        RDRenderer* r,
                                        const RDInstruction* instr, usize idx);
