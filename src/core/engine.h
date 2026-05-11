#pragma once

#include "core/registers.h"
#include "core/segment.h"
#include <redasm/redasm.h>

typedef enum {
    RD_EI_NONE = 0,
    RD_EI_FLOW,
    RD_EI_JUMP,
    RD_EI_CALL,
} RDEngineItemKind;

typedef struct RDEngineItem {
    RDEngineItemKind kind;
    RDConfidence confidence;
    RDAddress from;
    RDAddress address;
    RDRegisterHMap registers;
    char* name;
} RDEngineItem;

typedef struct RDEngineFlow {
    RDAddress value;
    bool has_value;
} RDEngineFlow;

typedef struct RDEngineQueue {
    RDEngineItem* data;
    usize length;
    usize capacity;
    usize head;
} RDEngineQueue;

bool rd_i_engine_enqueue_jump(RDContext* ctx, RDAddress address);
bool rd_i_engine_enqueue_call(RDContext* ctx, RDAddress address,
                              const char* name, RDConfidence c);
bool rd_i_engine_has_pending_code(const RDContext* ctx);
bool rd_i_engine_decode(RDContext* ctx, RDAddress address,
                        const RDSegmentFull* seg, usize index,
                        RDInstruction* instr);
u16 rd_i_engine_tick(RDContext* ctx);
void rd_i_engine_destroy(RDContext* ctx);
