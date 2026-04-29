#include "hooks.h"
#include "core/context.h"
#include "support/containers.h"
#include "support/logging.h"

static int _rd_instruction_hook_cmp(const void* a, const void* b) {
    const RDInstructionHookItem* ia = a;
    const RDInstructionHookItem* ib = b;
    if(ia->name < ib->name) return -1;
    if(ia->name > ib->name) return 1;
    return 0;
}

static int _rd_address_hook_cmp(const void* a, const void* b) {
    const RDAddressHookItem* ia = a;
    const RDAddressHookItem* ib = b;
    if(ia->name < ib->name) return -1;
    if(ia->name > ib->name) return 1;
    return 0;
}

static int _rd_xref_hook_cmp(const void* a, const void* b) {
    const RDXRefHookItem* ia = a;
    const RDXRefHookItem* ib = b;
    if(ia->name < ib->name) return -1;
    if(ia->name > ib->name) return 1;
    return 0;
}

static int _rd_render_hook_cmp(const void* a, const void* b) {
    const RDRenderHookItem* ia = a;
    const RDRenderHookItem* ib = b;
    if(ia->name < ib->name) return -1;
    if(ia->name > ib->name) return 1;
    return 0;
}

static int _rd_instruction_hook_search(const void* key, const void* elem) {
    const char* name = (const char*)key;
    const char* ename = ((const RDInstructionHookItem*)elem)->name;
    if(name < ename) return -1;
    if(name > ename) return 1;
    return 0;
}

static int _rd_address_hook_search(const void* key, const void* elem) {
    const char* name = (const char*)key;
    const char* ename = ((const RDAddressHookItem*)elem)->name;
    if(name < ename) return -1;
    if(name > ename) return 1;
    return 0;
}

static int _rd_xref_hook_search(const void* key, const void* elem) {
    const char* name = (const char*)key;
    const char* ename = ((const RDXRefHookItem*)elem)->name;
    if(name < ename) return -1;
    if(name > ename) return 1;
    return 0;
}

static int _rd_render_hook_search(const void* key, const void* elem) {
    const char* name = (const char*)key;
    const char* ename = ((const RDRenderHookItem*)elem)->name;
    if(name < ename) return -1;
    if(name > ename) return 1;
    return 0;
}

RDHooks* rd_i_hooks_create(void) { return calloc(1, sizeof(RDHooks)); }

void rd_i_hooks_destroy(RDHooks* self) {
    if(!self) return;
    vect_destroy(&self->instruction);
    vect_destroy(&self->address);
    vect_destroy(&self->xref);
    vect_destroy(&self->render);
    free(self);
}

bool rd_register_instruction_hook(RDContext* ctx, const char* name,
                                  RDInstructionHook h) {
    if(!name || !h) return false;

    const char* interned = rd_i_strpool_intern(&ctx->strings, name);

    size_t i = vect_lower_bound(interned, &ctx->hooks->instruction,
                                _rd_instruction_hook_search);

    // check for duplicate
    while(i < vect_length(&ctx->hooks->instruction) &&
          vect_at(&ctx->hooks->instruction, i)->name == interned) {
        if(vect_at(&ctx->hooks->instruction, i)->hook == h) {
            LOG_WARN("instruction hook '%s' already registered with same "
                     "handler, ignoring",
                     name);
            return false;
        }
        i++;
    }

    vect_push(&ctx->hooks->instruction, (RDInstructionHookItem){
                                            .name = interned,
                                            .hook = h,
                                        });

    vect_sort(&ctx->hooks->instruction, _rd_instruction_hook_cmp);
    return true;
}

bool rd_register_address_hook(RDContext* ctx, const char* name,
                              RDAddressHook h) {
    if(!name || !h) return false;

    const char* interned = rd_i_strpool_intern(&ctx->strings, name);

    size_t i = vect_lower_bound(interned, &ctx->hooks->address,
                                _rd_address_hook_search);

    // check for duplicate
    while(i < vect_length(&ctx->hooks->address) &&
          vect_at(&ctx->hooks->address, i)->name == interned) {
        if(vect_at(&ctx->hooks->address, i)->hook == h) {
            LOG_WARN("instruction hook '%s' already registered with same "
                     "handler, ignoring",
                     name);
            return false;
        }
        i++;
    }

    vect_push(&ctx->hooks->address, (RDAddressHookItem){
                                        .name = interned,
                                        .hook = h,
                                    });

    vect_sort(&ctx->hooks->address, _rd_address_hook_cmp);
    return true;
}

bool rd_register_xref_hook(RDContext* ctx, const char* name, RDXRefHook h) {
    if(!name || !h) return false;

    const char* interned = rd_i_strpool_intern(&ctx->strings, name);

    size_t i =
        vect_lower_bound(interned, &ctx->hooks->xref, _rd_xref_hook_search);

    // check for duplicate
    while(i < vect_length(&ctx->hooks->xref) &&
          vect_at(&ctx->hooks->xref, i)->name == interned) {
        if(vect_at(&ctx->hooks->xref, i)->hook == h) {
            LOG_WARN("instruction hook '%s' already registered with same "
                     "handler, ignoring",
                     name);
            return false;
        }
        i++;
    }

    vect_push(&ctx->hooks->xref, (RDXRefHookItem){
                                     .name = interned,
                                     .hook = h,
                                 });

    vect_sort(&ctx->hooks->xref, _rd_xref_hook_cmp);
    return true;
}

bool rd_register_render_mnemonic_hook(RDContext* ctx, const char* name,
                                      RDRenderMnemonicHook h) {
    if(!name || !h) return false;
    const char* interned = rd_i_strpool_intern(&ctx->strings, name);

    size_t i =
        vect_lower_bound(interned, &ctx->hooks->render, _rd_render_hook_search);

    if(i < vect_length(&ctx->hooks->render) &&
       vect_at(&ctx->hooks->render, i)->name == interned) {
        if(vect_at(&ctx->hooks->render, i)->mnemonic)
            LOG_WARN("render mnemonic hook '%s' already registered, replacing",
                     name);
        vect_at(&ctx->hooks->render, i)->mnemonic = h;
        return true;
    }

    vect_push(&ctx->hooks->render, (RDRenderHookItem){
                                       .name = interned,
                                       .mnemonic = h,
                                   });

    vect_sort(&ctx->hooks->render, _rd_render_hook_cmp);
    return true;
}

bool rd_register_render_operand_hook(RDContext* ctx, const char* name,
                                     RDRenderOperandHook h) {
    if(!name || !h) return false;

    const char* interned = rd_i_strpool_intern(&ctx->strings, name);

    size_t i =
        vect_lower_bound(interned, &ctx->hooks->render, _rd_render_hook_search);

    if(i < vect_length(&ctx->hooks->render) &&
       vect_at(&ctx->hooks->render, i)->name == interned) {
        if(vect_at(&ctx->hooks->render, i)->operand)
            LOG_WARN("render operand hook '%s' already registered, replacing",
                     name);

        vect_at(&ctx->hooks->render, i)->operand = h;
        return true;
    }

    vect_push(&ctx->hooks->render, (RDRenderHookItem){
                                       .name = interned,
                                       .operand = h,
                                   });
    vect_sort(&ctx->hooks->render, _rd_render_hook_cmp);
    return true;
}

void rd_fire_instruction_hook(RDContext* ctx, const char* name,
                              RDInstruction* instr) {
    if(!name || !instr) return;

    const char* interned = rd_i_strpool_intern(&ctx->strings, name);

    size_t i = vect_lower_bound(interned, &ctx->hooks->instruction,
                                _rd_instruction_hook_search);

    while(i < vect_length(&ctx->hooks->instruction) &&
          vect_at(&ctx->hooks->instruction, i)->name == interned) {
        vect_at(&ctx->hooks->instruction, i)->hook(ctx, instr);
        i++;
    }
}

void rd_fire_address_hook(RDContext* ctx, const char* name, RDAddress addr) {
    if(!name) return;

    const char* interned = rd_i_strpool_intern(&ctx->strings, name);

    size_t i = vect_lower_bound(interned, &ctx->hooks->address,
                                _rd_address_hook_search);

    while(i < vect_length(&ctx->hooks->address) &&
          vect_at(&ctx->hooks->address, i)->name == interned) {
        vect_at(&ctx->hooks->address, i)->hook(ctx, addr);
        i++;
    }
}

void rd_fire_xref_hook(RDContext* ctx, const char* name, RDAddress from,
                       RDAddress to, RDXRefType type) {
    if(!name) return;

    const char* interned = rd_i_strpool_intern(&ctx->strings, name);

    size_t i =
        vect_lower_bound(interned, &ctx->hooks->xref, _rd_xref_hook_search);

    while(i < vect_length(&ctx->hooks->xref) &&
          vect_at(&ctx->hooks->xref, i)->name == interned) {
        vect_at(&ctx->hooks->xref, i)->hook(ctx, from, to, type);
        i++;
    }
}

bool rd_fire_render_mnemonic_hook(RDContext* ctx, const char* name,
                                  RDRenderer* r, const RDInstruction* instr) {
    if(!name || !r || !instr) return false;
    const char* interned = rd_i_strpool_intern(&ctx->strings, name);

    size_t i =
        vect_lower_bound(interned, &ctx->hooks->render, _rd_render_hook_search);

    if(i < vect_length(&ctx->hooks->render) &&
       vect_at(&ctx->hooks->render, i)->name == interned &&
       vect_at(&ctx->hooks->render, i)->mnemonic) {
        vect_at(&ctx->hooks->render, i)->mnemonic(ctx, r, instr);
        return true;
    }

    return false;
}

bool rd_fire_render_operand_hook(RDContext* ctx, const char* name,
                                 RDRenderer* r, const RDInstruction* instr,
                                 usize idx) {
    if(!name || !r || !instr || idx >= RD_MAX_OPERANDS) return false;
    if(instr->operands[idx].kind == RD_OP_NULL) return false;

    const char* interned = rd_i_strpool_intern(&ctx->strings, name);

    size_t i =
        vect_lower_bound(interned, &ctx->hooks->render, _rd_render_hook_search);

    if(i < vect_length(&ctx->hooks->render) &&
       vect_at(&ctx->hooks->render, i)->name == interned &&
       vect_at(&ctx->hooks->render, i)->operand) {
        vect_at(&ctx->hooks->render, i)->operand(ctx, r, instr, idx);
        return true;
    }

    return false;
}
