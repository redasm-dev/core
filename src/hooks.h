#pragma once

#include <redasm/hooks.h>

typedef struct RDHookItem {
    const char* name;
    RDHook hook;
} RDHookItem;

typedef struct RDDecodeHookItem {
    const char* name;
    RDDecodeHook hook;
} RDDecodeHookItem;

typedef struct RDEmulateHookItem {
    const char* name;
    RDEmulateHook hook;
} RDEmulateHookItem;

typedef struct RDAddressHookItem {
    const char* name;
    RDAddressHook hook;
} RDAddressHookItem;

typedef struct RDStringHookItem {
    const char* name;
    RDStringHook hook;
} RDStringHookItem;

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
        RDHookItem* data;
        usize length;
        usize capacity;
    } general;

    struct {
        RDDecodeHookItem* data;
        usize length;
        usize capacity;
    } decode;

    struct {
        RDEmulateHookItem* data;
        usize length;
        usize capacity;
    } emulate;

    struct {
        RDAddressHookItem* data;
        usize length;
        usize capacity;
    } address;

    struct {
        RDStringHookItem* data;
        usize length;
        usize capacity;
    } string;

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
