#pragma once

#include <redasm/allocator.h>
#include <redasm/context.h>
#include <redasm/function.h>
#include <redasm/graph/graph.h>
#include <redasm/graph/layout.h>
#include <redasm/hooks.h>
#include <redasm/kb.h>
#include <redasm/mapping.h>
#include <redasm/plugins/analyzer.h>
#include <redasm/plugins/command.h>
#include <redasm/plugins/loader.h>
#include <redasm/plugins/processor/processor.h>
#include <redasm/rdil/rdil.h>
#include <redasm/registers.h>
#include <redasm/segment.h>
#include <redasm/support/logging.h>
#include <redasm/support/utils.h>
#include <redasm/surface/graph.h>
#include <redasm/surface/renderer.h>
#include <redasm/surface/surface.h>
#include <redasm/theme.h>
#include <redasm/types/def.h>
#include <redasm/types/type.h>

typedef struct RDTestResult RDTestResult;

typedef enum {
    RD_ACCEPT_OK = 0,
    RD_ACCEPT_FAIL,
    RD_ACCEPT_FAIL_WRITE,
} RDAcceptStatus;

typedef enum {
    RD_AM_NEW = 0,
    RD_AM_DATABASE,
    RD_AM_PROJECT,
} RDAcceptMode;

typedef struct RDAcceptResult {
    RDAcceptStatus status;
    RDContext* context;
} RDAcceptResult;

typedef struct RDAcceptParams {
    const RDProcessorPlugin* processorplugin;
    RDLoadAddressing addressing;
    RDAcceptMode mode;
    const char* db_path;
    int min_string;
} RDAcceptParams;

typedef struct RDInitParams {
    const char** kb_paths;
} RDInitParams;

typedef struct RDTestResultSlice {
    const RDTestResult** data;
    usize length;
} RDTestResultSlice;

typedef struct RDDecodedInstruction {
    RDInstruction instr;
    const char* instr_text;
    const char* mnemonic;
} RDDecodedInstruction;

// clang-format off
RD_API void rd_init(const RDInitParams* params);
RD_API void rd_deinit(void);
RD_API RDTestResultSlice rd_test_data(const char* data, usize n);
RD_API RDTestResultSlice rd_test(const char* filepath);
RD_API bool rd_module_load(const char* filepath);
RD_API RDAcceptResult rd_accept(const RDTestResult* tr, const RDAcceptParams* params);
RD_API void rd_reject(void);
RD_API const char* rd_dump_instruction(const RDInstruction* instr);
RD_API bool rd_decode_bytes(const char** bytes, usize* n, RDAddress* addr, const RDProcessorPlugin* p, RDDecodedInstruction* dec);

RD_API const RDLoaderPlugin* rd_testresult_get_loader_plugin(const RDTestResult* self);
RD_API const RDProcessorPlugin* rd_testresult_get_processor_plugin(const RDTestResult* self);
RD_API const char* rd_testresult_get_loader_name(const RDTestResult* self);
RD_API const char* rd_testresult_get_filepath(const RDTestResult* self);
// clang-format on
