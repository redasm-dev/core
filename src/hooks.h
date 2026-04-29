#pragma once

#include <redasm/hooks.h>

typedef struct RDInstructionHookItem {
    const char* name;
    RDInstructionHook hook;
} RDInstructionHookItem;

typedef struct RDAddressHookItem {
    const char* name;
    RDAddressHook hook;
} RDAddressHookItem;

typedef struct RDXRefHookItem {
    const char* name;
    RDXRefHook hook;
} RDXRefHookItem;

typedef struct RDRenderHookItem {
    const char* name;
    RDRenderMnemonicHook mnemonic;
    RDRenderOperandHook operand;
} RDRenderHookItem;

typedef struct RDHooks {
    struct {
        RDInstructionHookItem* data;
        usize length;
        usize capacity;
    } instruction;

    struct {
        RDAddressHookItem* data;
        usize length;
        usize capacity;
    } address;

    struct {
        RDXRefHookItem* data;
        usize length;
        usize capacity;
    } xref;

    struct {
        RDRenderHookItem* data;
        usize length;
        usize capacity;
    } render;
} RDHooks;

RDHooks* rd_i_hooks_create(void);
void rd_i_hooks_destroy(RDHooks* self);
