#include "chunk.h"
#include "core/context.h"

int rd_i_functionchunk_kcmp_pred(const void* key, const void* item) {
    RDRelAddress rel_address = *(const RDRelAddress*)key;
    const RDFunctionChunk* c = *(const RDFunctionChunk**)item;
    if(rel_address < c->start) return -1;
    if(rel_address >= c->end) return 1;
    return 0;
}

RDAddress rd_functionchunk_get_start(const RDFunctionChunk* self) {
    const RDContext* ctx = self->func->context;
    return rd_i_abs(ctx, self->start);
}

RDAddress rd_functionchunk_get_end(const RDFunctionChunk* self) {
    const RDContext* ctx = self->func->context;
    return rd_i_abs(ctx, self->end);
}

usize rd_functionchunk_get_instruction_count(const RDFunctionChunk* self) {
    return self->n_instructions;
}

bool rd_functionchunk_has_noret(const RDFunctionChunk* self) {
    return self->has_noret;
}
