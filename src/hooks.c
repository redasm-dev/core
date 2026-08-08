#include "hooks.h"
#include "core/context.h"
#include "support/containers.h"
#include <redasm/support/logging.h>

static int _rd_hookitem_search(const void* key, const void* elem) {
    const RDHookKey* k = (const RDHookKey*)key;
    const RDHookItem* e = (const RDHookItem*)elem;
    if(k->name != e->name) return k->name < e->name ? -1 : 1;
    if(k->kind != e->kind) return k->kind < e->kind ? -1 : 1;
    return 0;
}

static inline bool _rd_hookkind_is_exclusive(RDHookKind kind) {
    return kind == RD_HOOK_RENDER_MNEMONIC || kind == RD_HOOK_RENDER_OPERAND;
}

static bool _rd_fire_hook(RDContext* ctx, const RDHookEvent* e) {
    if(!e->name) return false;

    const char* interned = rd_i_strpool_intern(&ctx->strings, e->name);
    RDHookKey key = {.name = interned, .kind = e->kind};
    size_t i = vect_lower_bound(&ctx->hooks, &key, _rd_hookitem_search);
    size_t start = i;

    while(i < vect_length(&ctx->hooks) &&
          vect_at(&ctx->hooks, i)->name == interned &&
          vect_at(&ctx->hooks, i)->kind == e->kind)
        i++;

    usize n = i - start;
    if(!n) return false;

    // Snapshot just the matching sub-range before invoking anything.
    // A handler is untrusted plugin code and may reentrantly call
    // rd_register_hook (e.g. lazily registering a second listener on
    // first fire), that would vect_ins into this exact vector and shift
    // indices out from under a live iterator.
    // Duplicating decouples iteration from mutation entirely.
    RDHookItemVect sub = {
        .data = ctx->hooks.data + start,
        .length = n,
        .capacity = n,
    };

    RDHookItemVect snapshot = {0};
    vect_dup(&snapshot, &sub);

    const RDHookItem* item;
    vect_each(item, &snapshot) item->fn(ctx, e, item->userdata);

    vect_destroy(&snapshot);
    return true;
}

bool rd_register_hook(RDContext* ctx, RDHookKind kind, const char* name,
                      RDHookFunc h, void* userdata) {
    if(!name || !h) return false;

    const char* interned = rd_i_strpool_intern(&ctx->strings, name);

    RDHookKey key = {.name = interned, .kind = kind};
    size_t idx = vect_lower_bound(&ctx->hooks, &key, _rd_hookitem_search);

    if(_rd_hookkind_is_exclusive(kind)) {
        if(idx < vect_length(&ctx->hooks) &&
           vect_at(&ctx->hooks, idx)->name == interned &&
           vect_at(&ctx->hooks, idx)->kind == kind) {
            RD_LOG_WARN("hook '%s' already registered, replacing", name);
            vect_at(&ctx->hooks, idx)->fn = h;
            vect_at(&ctx->hooks, idx)->userdata = userdata;
            return true;
        }
    }
    else {
        while(idx < vect_length(&ctx->hooks) &&
              vect_at(&ctx->hooks, idx)->name == interned &&
              vect_at(&ctx->hooks, idx)->kind == kind) {
            if(vect_at(&ctx->hooks, idx)->fn == h &&
               vect_at(&ctx->hooks, idx)->userdata == userdata) {
                RD_LOG_WARN("hook '%s' already registered with same "
                            "handler, ignoring",
                            name);
                return false;
            }
            idx++;
        }
    }

    vect_ins(&ctx->hooks, idx,
             (RDHookItem){
                 .name = interned,
                 .kind = kind,
                 .fn = h,
                 .userdata = userdata,
             });

    return true;
}

void rd_fire_hook(RDContext* ctx, const char* name) {
    _rd_fire_hook(ctx, &(RDHookEvent){
                           .kind = RD_HOOK_GENERAL,
                           .name = name,
                       });
}

void rd_fire_decode_hook(RDContext* ctx, const char* name,
                         RDInstruction* instr) {
    if(!instr) return;

    _rd_fire_hook(ctx, &(RDHookEvent){
                           .kind = RD_HOOK_DECODE,
                           .name = name,
                           .decode = {.instr = instr},
                       });
}

void rd_fire_emulate_hook(RDContext* ctx, const char* name,
                          const RDInstruction* instr) {
    if(!instr) return;

    _rd_fire_hook(ctx, &(RDHookEvent){
                           .kind = RD_HOOK_EMULATE,
                           .name = name,
                           .emulate = {.instr = instr},
                       });
}

void rd_fire_address_hook(RDContext* ctx, const char* name, RDAddress addr) {
    _rd_fire_hook(ctx, &(RDHookEvent){
                           .kind = RD_HOOK_ADDRESS,
                           .name = name,
                           .addr = {.address = addr},
                       });
}

void rd_fire_str_hook(RDContext* ctx, const char* name, RDAddress addr,
                      const char* str, usize n) {
    _rd_fire_hook(ctx, &(RDHookEvent){
                           .kind = RD_HOOK_STR,
                           .name = name,
                           .str = {.address = addr, .s = str, .n = n},
                       });
}

void rd_fire_func_hook(RDContext* ctx, const char* name, const RDFunction* f,
                       usize index) {
    if(!f) return;

    _rd_fire_hook(ctx,
                  &(RDHookEvent){
                      .kind = RD_HOOK_FUNC,
                      .name = name,
                      .func = {.address = f->address, .f = f, .index = index},
                  });
}

void rd_fire_xref_hook(RDContext* ctx, const char* name, RDAddress from,
                       RDAddress to, RDXRefType type) {
    _rd_fire_hook(ctx, &(RDHookEvent){
                           .kind = RD_HOOK_XREF,
                           .name = name,
                           .xref =
                               {
                                   .from = from,
                                   .to = to,
                                   .type = type,
                               },
                       });
}

bool rd_fire_render_mnemonic_hook(RDContext* ctx, const char* name,
                                  RDRenderer* r, const RDInstruction* instr) {
    if(!r || !instr) return false;

    return _rd_fire_hook(ctx,
                         &(RDHookEvent){
                             .kind = RD_HOOK_RENDER_MNEMONIC,
                             .name = name,
                             .render_mnemonic = {.renderer = r, .instr = instr},
                         });
}

bool rd_fire_render_operand_hook(RDContext* ctx, const char* name,
                                 RDRenderer* r, const RDInstruction* instr,
                                 usize idx) {
    if(!r || !instr || idx >= RD_MAX_OPERANDS) return false;
    if(instr->operands[idx].kind == RD_OP_NULL) return false;

    return _rd_fire_hook(
        ctx, &(RDHookEvent){
                 .kind = RD_HOOK_RENDER_OPERAND,
                 .name = name,
                 .render_operand = {.renderer = r, .instr = instr, .idx = idx},
             });
}
