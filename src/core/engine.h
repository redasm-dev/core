#pragma once

#include "core/db/types.h"
#include <redasm/redasm.h>

bool rd_i_engine_enqueue_jump(RDContext* ctx, RDAddress address);
bool rd_i_engine_enqueue_call(RDContext* ctx, RDAddress address,
                              const char* name, RDConfidence c);
bool rd_i_engine_has_pending_code(const RDContext* ctx);
bool rd_i_engine_decode(RDContext* ctx, RDAddress address,
                        RDInstruction* instr);
u16 rd_i_engine_tick(RDContext* ctx);
