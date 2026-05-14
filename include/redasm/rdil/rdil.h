#pragma once

#include <redasm/common.h>
#include <redasm/function.h>
#include <redasm/plugins/processor/instruction.h>
#include <redasm/rdil/opcodes.h>
#include <redasm/registers.h>

typedef struct RDIL RDIL;

typedef struct RDILInstructionSlice {
    const RDInstruction* data;
    usize length;
    usize instruction_length;
} RDILInstructionSlice;

// clang-format off
RD_API RDILInstructionSlice rd_lift(RDContext* ctx, RDAddress address);
RD_API RDInstruction* rd_il_push_instr(RDInstructionVect* self, RDILStatement s);
RD_API bool rd_il_instr_match(const RDContext* ctx, const RDILInstructionSlice* il,
                              const RDInstruction* shapes, usize count);

RD_API RDIL* rd_il_create(RDContext* ctx, const RDFunction* f);
RD_API void rd_il_destroy(RDIL* self);
RD_API void rd_il_flush(RDIL* self);
RD_API void rd_il_assign(RDIL* self, const RDFunction* f);
RD_API bool rd_il_run(RDIL* self);
RD_API bool rd_il_step(RDIL* self);
RD_API RDAddress rd_il_current_address(const RDIL* self);
RD_API RDILInstructionSlice rd_il_current_il(const RDIL* self);
RD_API const RDInstruction* rd_il_current_instr(const RDIL* self);
RD_API bool rd_il_get_target(const RDIL* self, RDAddress* target);
RD_API bool rd_il_get_regval(const RDIL* self, const char* regname, RDRegValue* value);
RD_API bool rd_il_set_regval(RDIL* self, const char* regname, RDRegValue value);
RD_API bool rd_il_del_regval(RDIL* self, const char* regname);
RD_API bool rd_il_get_regval_id(const RDIL* self, RDReg id, RDRegValue* value);
RD_API bool rd_il_set_regval_id(RDIL* self, RDReg id, RDRegValue value);
RD_API bool rd_il_del_regval_id(RDIL* self, RDReg id);
// clang-format on
