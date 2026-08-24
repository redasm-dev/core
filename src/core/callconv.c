#include "callconv.h"
#include "context.h"
#include <redasm/allocator.h>

RDCallConv* rd_i_callconv_create(const char* name, RDContext* ctx) {
    if(!name) return NULL;

    RDCallConv* self = rd_alloc(sizeof(*self));

    *self = (RDCallConv){
        .name = rd_i_strpool_intern(&ctx->strings, name),
    };

    return self;
}

void rd_i_callconv_destroy(RDCallConv* self) {
    vect_destroy(&self->arg_regs);
    rd_free(self);
}

const RDCallConv* rd_i_callconv_find(const RDContext* ctx, const char* name) {
    if(!name) return NULL;

    RDCallConv** cc;
    vect_each(cc, &ctx->callconvs) {
        if(!strcmp((*cc)->name, name)) return *cc;
    }

    return NULL;
}
