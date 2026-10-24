#pragma once

#include <redasm/config.h>
#include <redasm/context.h>
#include <redasm/plugins/processor/instruction.h>
#include <redasm/surface/renderer.h>

typedef enum {
    RD_HOOK_GENERAL = 0,
    RD_HOOK_DECODE,
    RD_HOOK_EMULATE,
    RD_HOOK_ADDRESS,
    RD_HOOK_STR,
    RD_HOOK_XREF,
    RD_HOOK_BUS,
    RD_HOOK_RENDER_MNEMONIC,
    RD_HOOK_RENDER_OPERAND,
} RDHookKind;

// clang-format off
typedef struct RDHookEvent {
    RDHookKind kind;
    const char* name; 

    union {
        struct { RDInstruction* instr; } decode;
        struct { const RDInstruction* instr; } emulate;
        struct { RDAddress address; } addr;
        struct { RDAddress address; const char* s; usize n; } str;
        struct { RDAddress from, to; RDXRefType type; } xref;
        struct { RDRenderer* renderer; const RDInstruction* instr; } render_mnemonic;
        struct { RDRenderer* renderer; const RDInstruction* instr; usize idx; } render_operand;
        struct { RDAddress address; usize size; bool has_value; u64 value; } bus;
    };
} RDHookEvent;
// clang-format on

typedef void (*RDHookFunc)(RDContext*, const RDHookEvent*, void* userdata);

RD_API bool rd_register_hook(RDContext* ctx, RDHookKind kind, const char* name,
                             RDHookFunc h, void* userdata);

RD_API void rd_emit_hook(RDContext* ctx, const char* name);
RD_API void rd_emit_decode_hook(RDContext* ctx, const char* name,
                                RDInstruction*);
RD_API void rd_emit_emulate_hook(RDContext* ctx, const char* name,
                                 const RDInstruction*);
RD_API void rd_emit_address_hook(RDContext* ctx, const char* name,
                                 RDAddress addr);
RD_API void rd_emit_str_hook(RDContext* ctx, const char* name, RDAddress addr,
                             const char* str, usize n);
RD_API void rd_emit_xref_hook(RDContext* ctx, const char* name, RDAddress from,
                              RDAddress to, RDXRefType type);
RD_API bool rd_emit_render_mnemonic_hook(RDContext*, const char* name,
                                         RDRenderer* r,
                                         const RDInstruction* instr);
RD_API bool rd_emit_render_operand_hook(RDContext*, const char* name,
                                        RDRenderer* r,
                                        const RDInstruction* instr, usize idx);

RD_API void rd_bus_write(RDContext* ctx, RDAddress addr, usize size, u64 value);
RD_API void rd_bus_write_unknown(RDContext* ctx, RDAddress addr, usize size);
