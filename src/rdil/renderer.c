#include "renderer.h"
#include "plugins/processor/processor.h"
#include "surface/renderer.h"
#include <redasm/rdil/opcodes.h>

static RDThemeKind _rd_il_stmt_color(RDILStatement stmt) {
    switch(stmt) {
        case RD_IL_RET: return RD_THEME_STOP;
        case RD_IL_JUMP: return RD_THEME_JUMP;
        case RD_IL_CALL: return RD_THEME_CALL;

        case RD_IL_UNKNOWN:
        case RD_IL_NOP: return RD_THEME_MUTED;

        case RD_IL_JUMP_EQ:
        case RD_IL_JUMP_NE:
        case RD_IL_JUMP_LT:
        case RD_IL_JUMP_LE:
        case RD_IL_JUMP_GT:
        case RD_IL_JUMP_GE: return RD_THEME_JUMP_COND;

        case RD_IL_CALL_EQ:
        case RD_IL_CALL_NE:
        case RD_IL_CALL_LT:
        case RD_IL_CALL_LE:
        case RD_IL_CALL_GT:
        case RD_IL_CALL_GE: return RD_THEME_CALL_COND;

        default: break;
    }

    return RD_THEME_FOREGROUND;
}

void rd_i_il_render(RDRenderer* r, const RDInstructionVect* v) {
    if(vect_is_empty(v)) {
        rd_renderer_muted(r, "unknown");
        return;
    }

    bool first = true;
    RDInstruction* instr;

    vect_each(instr, v) {
        if(!first) rd_renderer_norm(r, "; ");

        rd_renderer_text(r, instr->mnemonic,
                         _rd_il_stmt_color((RDILStatement)instr->id),
                         RD_THEME_DEFAULT);

        rd_renderer_ws(r, 1);

        rd_foreach_operand(i, op, instr) {
            if(i > 0) rd_renderer_norm(r, ", ");
            rd_i_processor_render_operand(r, instr, i, NULL);
        }

        first = false;
    }
}
