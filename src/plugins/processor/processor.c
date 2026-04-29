#include "processor.h"
#include "core/context.h"
#include "support/containers.h"
#include "support/logging.h"

RDContextProcessor rd_i_processor_create(const RDProcessorPlugin* p) {
    assert(p && "cannot create context processor");

    return (RDContextProcessor){
        .plugin = p,
        .instance = p->create ? p->create(p) : NULL,
    };
}

void rd_i_processor_destroy(RDContextProcessor* self) {
    if(self->plugin && self->plugin->destroy)
        self->plugin->destroy(self->instance);
}

bool rd_i_processor_resolve(RDContext* ctx) {
    const RDProcessorPlugin* curr = ctx->processor.plugin;

    while(curr && curr->parent) {
        const RDProcessorPlugin* p = rd_processor_find(curr->parent);

        if(!p) {
            LOG_FAIL("unknown parent processor '%s'", curr->parent);
            return false;
        }

        // cycle detection 1: check agains root
        if(p == ctx->processor.plugin) {
            LOG_FAIL("cycle detected in processor chain '%s'", p->id);
            return false;
        }

        // cycle detection 2: check if p is already in the chain
        RDContextProcessor* cp;
        vect_each(cp, &ctx->processor_chain) {
            if(cp->plugin == p) {
                LOG_FAIL("cycle detected in processor chain '%s'", p->id);
                return false;
            }
        }

        vect_push(&ctx->processor_chain, rd_i_processor_create(p));
        curr = p;
    }

    return true;
}

u32 rd_processor_get_flags(const RDContext* ctx) {
    if(ctx->processor.plugin->flags) return ctx->processor.plugin->flags;

    RDContextProcessor* cp;
    vect_each(cp, &ctx->processor_chain) {
        if(cp->plugin->flags) return cp->plugin->flags;
    }

    return 0;
}

const char* rd_processor_get_id(const RDContext* ctx) {
    return ctx->processor.plugin->id;
}

const char* rd_processor_get_name(const RDContext* ctx) {
    return ctx->processor.plugin->name;
}

const char* rd_processor_get_operand_sep(const RDContext* ctx) {
    if(ctx->processor.plugin->operand_sep)
        return ctx->processor.plugin->operand_sep;

    RDContextProcessor* cp;
    vect_each(cp, &ctx->processor_chain) {
        if(cp->plugin->operand_sep) return cp->plugin->operand_sep;
    }

    return ", ";
}

unsigned int rd_processor_get_code_ptr_size(const RDContext* ctx) {
    if(ctx->processor.plugin->code_ptr_size)
        return ctx->processor.plugin->code_ptr_size;

    RDContextProcessor* cp;
    vect_each(cp, &ctx->processor_chain) {
        if(cp->plugin->code_ptr_size) return cp->plugin->code_ptr_size;
    }

    return 0;
}

unsigned int rd_processor_get_ptr_size(const RDContext* ctx) {
    if(ctx->processor.plugin->ptr_size) return ctx->processor.plugin->ptr_size;

    RDContextProcessor* cp;
    vect_each(cp, &ctx->processor_chain) {
        if(cp->plugin->ptr_size) return cp->plugin->ptr_size;
    }

    return 0;
}

unsigned int rd_processor_get_int_size(const RDContext* ctx) {
    if(ctx->processor.plugin->int_size) return ctx->processor.plugin->int_size;

    RDContextProcessor* cp;
    vect_each(cp, &ctx->processor_chain) {
        if(cp->plugin->int_size) return cp->plugin->int_size;
    }

    return 0;
}

bool rd_i_processor_has_emulate(const RDContext* ctx) {
    if(ctx->processor.plugin->emulate) return true;

    RDContextProcessor* cp;
    vect_each(cp, &ctx->processor_chain) {
        if(cp->plugin->emulate) return true;
    }

    return false;
}

bool rd_i_processor_has_lift(const RDContext* ctx) {
    if(ctx->processor.plugin->lift) return true;

    RDContextProcessor* cp;
    vect_each(cp, &ctx->processor_chain) {
        if(cp->plugin->lift) return true;
    }

    return false;
}

bool rd_i_processor_has_render_segment(const RDContext* ctx) {
    if(ctx->processor.plugin->render_segment) return true;

    RDContextProcessor* cp;
    vect_each(cp, &ctx->processor_chain) {
        if(cp->plugin->render_segment) return true;
    }

    return false;
}

bool rd_i_processor_has_render_function(const RDContext* ctx) {
    if(ctx->processor.plugin->render_function) return true;

    RDContextProcessor* cp;
    vect_each(cp, &ctx->processor_chain) {
        if(cp->plugin->render_function) return true;
    }

    return false;
}

void rd_i_processor_decode(const RDContext* ctx, RDInstruction* instr) {
    if(ctx->processor.plugin->decode)
        ctx->processor.plugin->decode(ctx, instr, ctx->processor.instance);
    else
        rd_processor_parent_decode(ctx, instr);
}

void rd_i_processor_emulate(RDContext* ctx, const RDInstruction* instr) {
    if(ctx->processor.plugin->emulate)
        ctx->processor.plugin->emulate(ctx, instr, ctx->processor.instance);
    else
        rd_processor_parent_emulate(ctx, instr);
}

void rd_i_processor_lift(const RDContext* ctx, const RDInstruction* instr,
                         RDILInstruction* il) {
    if(ctx->processor.plugin->lift)
        ctx->processor.plugin->lift(ctx, instr, il, ctx->processor.instance);
    else
        rd_processor_parent_lift(ctx, instr, il);
}

const char* rd_i_processor_get_mnemonic(const RDContext* ctx,
                                        const RDInstruction* instr) {
    if(ctx->processor.plugin->get_mnemonic) {
        return ctx->processor.plugin->get_mnemonic(instr,
                                                   ctx->processor.instance);
    }

    return rd_processor_parent_get_mnemonic(ctx, instr);
}

const char* rd_i_processor_get_register(RDContext* ctx, int r) {
    if(ctx->processor.plugin->get_register)
        return ctx->processor.plugin->get_register(r, ctx->processor.instance);

    return rd_processor_parent_get_register(ctx, r);
}

const char** rd_i_processor_get_prologues(RDContext* ctx) {
    if(ctx->processor.plugin->get_prologues) {
        return ctx->processor.plugin->get_prologues(ctx->processor.instance,
                                                    ctx);
    }

    return rd_processor_parent_get_prologues(ctx);
}

void rd_i_processor_render_segment(RDContext* ctx, RDRenderer* r,
                                   const RDSegment* s) {
    if(ctx->processor.plugin->render_segment)
        ctx->processor.plugin->render_segment(r, s, ctx->processor.instance);
    else
        rd_processor_parent_render_segment(ctx, r, s);
}

void rd_i_processor_render_function(RDContext* ctx, RDRenderer* r,
                                    const RDFunction* f) {
    if(ctx->processor.plugin->render_function)
        ctx->processor.plugin->render_function(r, f, ctx->processor.instance);
    else
        rd_processor_parent_render_function(ctx, r, f);
}

void rd_i_processor_render_mnemonic(RDContext* ctx, RDRenderer* r,
                                    const RDInstruction* instr) {
    if(ctx->processor.plugin->render_mnemonic) {
        ctx->processor.plugin->render_mnemonic(r, instr,
                                               ctx->processor.instance);
    }
    else
        rd_processor_parent_render_mnemonic(ctx, r, instr);
}

void rd_i_processor_render_operand(RDContext* ctx, RDRenderer* r,
                                   const RDInstruction* instr, usize idx) {
    if(ctx->processor.plugin->render_operand) {
        ctx->processor.plugin->render_operand(r, instr, idx,
                                              ctx->processor.instance);
    }
    else
        rd_processor_parent_render_operand(ctx, r, instr, idx);
}

void rd_processor_parent_decode(const RDContext* ctx, RDInstruction* instr) {
    RDContextProcessor* cp;
    vect_each(cp, &ctx->processor_chain) {
        if(cp->plugin->decode) {
            cp->plugin->decode(ctx, instr, cp->instance);
            break;
        }
    }
}

void rd_processor_parent_emulate(RDContext* ctx, const RDInstruction* instr) {
    RDContextProcessor* cp;
    vect_each(cp, &ctx->processor_chain) {
        if(cp->plugin->emulate) {
            cp->plugin->emulate(ctx, instr, cp->instance);
            break;
        }
    }
}

void rd_processor_parent_lift(const RDContext* ctx, const RDInstruction* instr,
                              RDILInstruction* il) {
    RDContextProcessor* cp;
    vect_each(cp, &ctx->processor_chain) {
        if(cp->plugin->lift) {
            cp->plugin->lift(ctx, instr, il, cp->instance);
            break;
        }
    }
}

const char* rd_processor_parent_get_mnemonic(const RDContext* ctx,
                                             const RDInstruction* instr) {
    RDContextProcessor* cp;
    vect_each(cp, &ctx->processor_chain) {
        if(cp->plugin->get_mnemonic)
            return cp->plugin->get_mnemonic(instr, cp->instance);
    }

    return NULL;
}

const char* rd_processor_parent_get_register(const RDContext* ctx, int r) {
    RDContextProcessor* cp;
    vect_each(cp, &ctx->processor_chain) {
        if(cp->plugin->get_register)
            return cp->plugin->get_register(r, cp->instance);
    }

    return NULL;
}

const char** rd_processor_parent_get_prologues(const RDContext* ctx) {
    RDContextProcessor* cp;
    vect_each(cp, &ctx->processor_chain) {
        if(cp->plugin->get_prologues)
            return cp->plugin->get_prologues(cp->instance, ctx);
    }

    return NULL;
}

void rd_processor_parent_render_segment(RDContext* ctx, RDRenderer* r,
                                        const RDSegment* s) {
    RDContextProcessor* cp;
    vect_each(cp, &ctx->processor_chain) {
        if(cp->plugin->render_segment) {
            cp->plugin->render_segment(r, s, cp->instance);
            break;
        }
    }
}

void rd_processor_parent_render_function(RDContext* ctx, RDRenderer* r,
                                         const RDFunction* f) {
    RDContextProcessor* cp;
    vect_each(cp, &ctx->processor_chain) {
        if(cp->plugin->render_function) {
            cp->plugin->render_function(r, f, cp->instance);
            break;
        }
    }
}

void rd_processor_parent_render_mnemonic(RDContext* ctx, RDRenderer* r,
                                         const RDInstruction* instr) {
    RDContextProcessor* cp;
    vect_each(cp, &ctx->processor_chain) {
        if(cp->plugin->render_mnemonic) {
            cp->plugin->render_mnemonic(r, instr, cp->instance);
            break;
        }
    }
}

RD_API void rd_processor_parent_render_operand(RDContext* ctx, RDRenderer* r,
                                               const RDInstruction* instr,
                                               usize idx) {
    RDContextProcessor* cp;
    vect_each(cp, &ctx->processor_chain) {
        if(cp->plugin->render_operand) {
            cp->plugin->render_operand(r, instr, idx, cp->instance);
            break;
        }
    }
}
