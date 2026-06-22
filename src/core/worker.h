#pragma once

// clang-format off
typedef enum {
    RD_WS_INIT = 0,

    // 1st pass
    RD_WS_EMULATE1, RD_WS_ANALYZE1, 
    RD_WS_MERGEDATA1, 

    // 2nd pass
    RD_WS_EMULATE2, RD_WS_ANALYZE2, 
    RD_WS_MERGEDATA2, 

    // Finalize pass
    RD_WS_FINALIZE, RD_WS_DONE, 

    RD_WS_COUNT, 
} RDWorkerSteps;
// clang-format on
