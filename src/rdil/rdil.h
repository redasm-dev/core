#pragma once

#include <redasm/config.h>
#include <redasm/plugins/processor/rdil.h>
#include <redasm/surface/renderer.h>

typedef struct RDILInstruction {
    u8* data;
    usize length;
    usize capacity;
} RDILInstruction;

void rd_i_il_init(RDILInstruction* self);
void rd_i_il_deinit(RDILInstruction* self);
void rd_i_il_render(RDRenderer* r, const RDILInstruction* il);
