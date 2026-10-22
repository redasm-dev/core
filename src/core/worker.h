#pragma once

#define RD_WORKER_STEPS(X)                                                     \
    X(RD_WS_INIT, "Init")                                                      \
    X(RD_WS_RECONCILE, "Reconcile")                                            \
    X(RD_WS_EMULATE, "Emulate")                                                \
    X(RD_WS_ANALYZE, "Analyze")                                                \
    X(RD_WS_SYMBOLS, "Symbols")                                                \
    X(RD_WS_FINALIZE, "Finalize")                                              \
    X(RD_WS_DONE, "Done")

#define RD_WORKER_STEP_ENUM_X(id, name) id,

#define RD_WORKER_STEP_CASE_X(id, name)                                        \
    case id: return name;

// clang-format off
typedef enum {
    RD_WORKER_STEPS(RD_WORKER_STEP_ENUM_X)
    RD_WS_COUNT, 
} RDWorkerSteps;
// clang-format on

const char* rd_i_worker_get_step_name(RDWorkerSteps s);
