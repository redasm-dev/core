#include "hooks.h"
#include "core/context.h"
#include "support/containers.h"
#include "support/logging.h"

#define rd_fire_all_hooks_impl(it, ctx, name, hooks, searchcb, ...)            \
    do {                                                                       \
        if(!name) return;                                                      \
                                                                               \
        const char* interned = rd_i_strpool_intern(&ctx->strings, name);       \
        size_t _i = vect_lower_bound(interned, hooks, searchcb);               \
                                                                               \
        while(_i < vect_length(hooks) &&                                       \
              vect_at(hooks, _i)->name == interned) {                          \
            it = vect_at(hooks, _i);                                           \
            __VA_ARGS__                                                        \
            _i++;                                                              \
        }                                                                      \
    } while(0)

#define rd_fire_one_hook_impl(it, ctx, name, hooks, searchcb, ...)             \
    do {                                                                       \
        if(!name) return false;                                                \
        const char* interned = rd_i_strpool_intern(&ctx->strings, name);       \
        size_t _i = vect_lower_bound(interned, hooks, searchcb);               \
                                                                               \
        if(_i < vect_length(hooks) && vect_at(hooks, _i)->name == interned) {  \
            it = vect_at(hooks, _i);                                           \
            __VA_ARGS__                                                        \
        }                                                                      \
    } while(0)

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

static int _rd_hook_search(const void* key, const void* elem) {
    const char* name = (const char*)key;
    const char* ename = ((const RDHookItem*)elem)->name;
    if(name < ename) return -1;
    if(name > ename) return 1;
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
    vect_destroy(&self->general);
    vect_destroy(&self->instruction);
    vect_destroy(&self->address);
    vect_destroy(&self->xref);
    vect_destroy(&self->render);
    free(self);
}

bool rd_register_hook(RDContext* ctx, const char* name, RDHook h) {
    if(!name || !h) return false;

    const char* interned = rd_i_strpool_intern(&ctx->strings, name);

    size_t i =
        vect_lower_bound(interned, &ctx->hooks->general, _rd_hook_search);

    // check for duplicate
    while(i < vect_length(&ctx->hooks->general) &&
          vect_at(&ctx->hooks->general, i)->name == interned) {
        if(vect_at(&ctx->hooks->general, i)->hook == h) {
            LOG_WARN("hook '%s' already registered with same handler, ignoring",
                     name);
            return false;
        }
        i++;
    }

    vect_push(&ctx->hooks->general, (RDHookItem){
                                        .name = interned,
                                        .hook = h,
                                    });

    vect_sort(&ctx->hooks->general, _rd_instruction_hook_cmp);
    return true;
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
            LOG_WARN("address hook '%s' already registered with same handler, "
                     "ignoring",
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
            LOG_WARN(
                "xref hook '%s' already registered with same handler, ignoring",
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

void rd_fire_hook(RDContext* ctx, const char* name) {
    RDHookItem* item;
    rd_fire_all_hooks_impl(item, ctx, name, &ctx->hooks->general,
                           _rd_hook_search, { item->hook(ctx); });
}

void rd_fire_instruction_hook(RDContext* ctx, const char* name,
                              RDInstruction* instr) {
    if(!instr) return;

    RDInstructionHookItem* item;
    rd_fire_all_hooks_impl(item, ctx, name, &ctx->hooks->instruction,
                           _rd_instruction_hook_search,
                           { item->hook(ctx, instr); });
}

void rd_fire_address_hook(RDContext* ctx, const char* name, RDAddress addr) {
    RDAddressHookItem* item;
    rd_fire_all_hooks_impl(item, ctx, name, &ctx->hooks->address,
                           _rd_instruction_hook_search,
                           { item->hook(ctx, addr); });
}

void rd_fire_xref_hook(RDContext* ctx, const char* name, RDAddress from,
                       RDAddress to, RDXRefType type) {
    RDXRefHookItem* item;
    rd_fire_all_hooks_impl(item, ctx, name, &ctx->hooks->xref,
                           _rd_xref_hook_search,
                           { item->hook(ctx, from, to, type); });
}

bool rd_fire_render_mnemonic_hook(RDContext* ctx, const char* name,
                                  RDRenderer* r, const RDInstruction* instr) {
    if(!r || !instr) return false;

    bool res = false;
    RDRenderHookItem* it;

    rd_fire_one_hook_impl(it, ctx, name, &ctx->hooks->render,
                          _rd_render_hook_search, {
                              if(it->mnemonic) {
                                  it->mnemonic(ctx, r, instr);
                                  res = true;
                              }
                          });

    return res;
}

bool rd_fire_render_operand_hook(RDContext* ctx, const char* name,
                                 RDRenderer* r, const RDInstruction* instr,
                                 usize idx) {
    if(!r || !instr || idx >= RD_MAX_OPERANDS) return false;
    if(instr->operands[idx].kind == RD_OP_NULL) return false;

    bool res = false;
    RDRenderHookItem* it;

    rd_fire_one_hook_impl(it, ctx, name, &ctx->hooks->render,
                          _rd_render_hook_search, {
                              if(it->operand) {
                                  it->operand(ctx, r, instr, idx);
                                  res = true;
                              }
                          });

    return res;
}
