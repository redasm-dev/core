#pragma once

// clang-format off
typedef enum {
    RD_WS_INIT = 0, RD_WS_RECONCILE,

    RD_WS_EMULATE, RD_WS_ANALYZE, RD_WS_SYMBOLS, 
    RD_WS_FINALIZE, RD_WS_DONE, 

    RD_WS_COUNT, 
} RDWorkerSteps;
// clang-format on
